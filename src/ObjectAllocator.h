/**
 * @file ObjectAllocator.cpp
 * @author Edgar Jose Donoso Mansilla (e.donosomansilla)
 * @course CS280
 * @term Spring 2025
 *
 * @brief Implementation for a basic memory manager
 */

//---------------------------------------------------------------------------
#ifndef OBJECTALLOCATORH
#define OBJECTALLOCATORH
//---------------------------------------------------------------------------

#include <string>

// If the client doesn't specify these:
static const int DEFAULT_OBJECTS_PER_PAGE = 4;
static const int DEFAULT_MAX_PAGES = 3;

/*!
  Exception class
*/
class OAException {
public:
  /*!
    Possible exception codes
  */
  enum OA_EXCEPTION {
    E_NO_MEMORY, //!< out of physical memory (operator new fails)
    E_NO_PAGES, //!< out of logical memory (max pages has been reached)
    E_BAD_BOUNDARY, //!< block address is on a page, but not on any block-boundary
    E_MULTIPLE_FREE, //!< block has already been freed
    E_CORRUPTED_BLOCK //!< block has been corrupted (pad bytes have been overwritten)
  };

  /*!
    Constructor

    \param ErrCode
      One of the 5 error codes listed above

    \param Message
      A message returned by the what method.
  */
  OAException(OA_EXCEPTION ErrCode, const std::string &Message) : error_code_(ErrCode), message_(Message) {};

  /*!
    Destructor
  */
  virtual ~OAException() {}

  /*!
    Retrieves the error code

    \return
      One of the 5 error codes.
  */
  OA_EXCEPTION code() const { return error_code_; }

  /*!
    Retrieves a human-readable string regarding the error.

    \return
      The NUL-terminated string representing the error.
  */
  virtual const char *what() const { return message_.c_str(); }

private:
  OA_EXCEPTION error_code_; //!< The error code (one of the 5)
  std::string message_; //!< The formatted string for the user.
};

/*!
  ObjectAllocator configuration parameters
*/
struct OAConfig {
  static const size_t BASIC_HEADER_SIZE = sizeof(unsigned) + 1; //!< allocation number + flags
  static const size_t EXTERNAL_HEADER_SIZE = sizeof(void *); //!< just a pointer

  /*!
    The different types of header blocks
  */
  enum HBLOCK_TYPE { hbNone, hbBasic, hbExtended, hbExternal };

  /*!
    POD that stores the information related to the header blocks.
  */
  struct HeaderBlockInfo {
    HBLOCK_TYPE type_; //!< Which of the 4 header types to use?
    size_t size_; //!< The size of this header
    size_t additional_; //!< How many user-defined additional bytes

    /*!
      Constructor

      \param type
        The kind of header blocks in use.

      \param additional
        The number of user-defined additional bytes required.

    */
    HeaderBlockInfo(HBLOCK_TYPE type = hbNone, unsigned additional = 0) :
        type_(type), size_(0), additional_(additional) {
      if (type_ == hbBasic)
        size_ = BASIC_HEADER_SIZE;
      else if (type_ == hbExtended) // alloc # + use counter + flag byte + user-defined
        size_ = sizeof(unsigned int) + sizeof(unsigned short) + sizeof(char) + additional_;
      else if (type_ == hbExternal)
        size_ = EXTERNAL_HEADER_SIZE;
    };
  };

  /*!
    Constructor

    \param UseCPPMemManager
      Determines whether or not to by-pass the OA.

    \param ObjectsPerPage
      Number of objects for each page of memory.

    \param MaxPages
      Maximum number of pages before throwing an exception. A value
      of 0 means unlimited.

    \param DebugOn
      Is debugging code on or off?

    \param PadBytes
      The number of bytes to the left and right of a block to pad with.

    \param HBInfo
      Information about the header blocks used

    \param Alignment
      The number of bytes to align on.
  */
  OAConfig(
      bool UseCPPMemManager = false,
      unsigned ObjectsPerPage = DEFAULT_OBJECTS_PER_PAGE,
      unsigned MaxPages = DEFAULT_MAX_PAGES,
      bool DebugOn = false,
      unsigned PadBytes = 0,
      const HeaderBlockInfo &HBInfo = HeaderBlockInfo(),
      unsigned Alignment = 0) :
      UseCPPMemManager_(UseCPPMemManager), ObjectsPerPage_(ObjectsPerPage), MaxPages_(MaxPages), DebugOn_(DebugOn),
      PadBytes_(PadBytes), HBlockInfo_(HBInfo), Alignment_(Alignment) {
    HBlockInfo_ = HBInfo;
    LeftAlignSize_ = 0;
    InterAlignSize_ = 0;
  }

  bool UseCPPMemManager_; //!< by-pass the functionality of the OA and use new/delete
  unsigned ObjectsPerPage_; //!< number of objects on each page
  unsigned MaxPages_; //!< maximum number of pages the OA can allocate (0=unlimited)
  bool DebugOn_; //!< enable/disable debugging code (signatures, checks, etc.)
  unsigned PadBytes_; //!< size of the left/right padding for each block
  HeaderBlockInfo HBlockInfo_; //!< size of the header for each block (0=no headers)
  unsigned Alignment_; //!< address alignment of each block
  unsigned LeftAlignSize_; //!< number of alignment bytes required to align first block
  unsigned InterAlignSize_; //!< number of alignment bytes required between remaining blocks
};

/*!
  POD that holds the ObjectAllocator statistical info
*/
struct OAStats {
  /*!
    Constructor
  */
  OAStats() :
      ObjectSize_(0), PageSize_(0), FreeObjects_(0), ObjectsInUse_(0), PagesInUse_(0), MostObjects_(0), Allocations_(0),
      Deallocations_(0) {};

  size_t ObjectSize_; //!< size of each object
  size_t PageSize_; //!< size of a page including all headers, padding, etc.
  unsigned FreeObjects_; //!< number of objects on the free list
  unsigned ObjectsInUse_; //!< number of objects in use by client
  unsigned PagesInUse_; //!< number of pages allocated
  unsigned MostObjects_; //!< most objects in use by client at one time
  unsigned Allocations_; //!< total requests to allocate memory
  unsigned Deallocations_; //!< total requests to free memory
};

/*!
  This allows us to easily treat raw objects as nodes in a linked list
*/
struct GenericObject {
  GenericObject *Next; //!< The next object in the list
};

/*!
  This is used with external headers
*/
struct MemBlockInfo {
  bool in_use; //!< Is the block free or in use?
  char *label; //!< A dynamically allocated NUL-terminated string
  unsigned alloc_num; //!< The allocation number (count) of this block
};

/*!
  This class represents a custom memory manager
*/
class ObjectAllocator {
public:
  // Defined by the client (pointer to a block, size of block)
  typedef void (*DUMPCALLBACK)(const void *, size_t); //!< Callback function when dumping memory leaks
  typedef void (*VALIDATECALLBACK)(const void *, size_t); //!< Callback function when validating blocks

  // Predefined values for memory signatures
  static const unsigned char UNALLOCATED_PATTERN = 0xAA; //!< New memory never given to the client
  static const unsigned char ALLOCATED_PATTERN = 0xBB; //!< Memory owned by the client
  static const unsigned char FREED_PATTERN = 0xCC; //!< Memory returned by the client
  static const unsigned char PAD_PATTERN = 0xDD; //!< Pad signature to detect buffer over/under flow
  static const unsigned char ALIGN_PATTERN = 0xEE; //!< For the alignment bytes

  // Creates the ObjectManager per the specified values
  // Throws an exception if the construction fails. (Memory allocation problem)
  ObjectAllocator(size_t ObjectSize, const OAConfig &config);

  // Destroys the ObjectManager (never throws)
  ~ObjectAllocator();

  // Take an object from the free list and give it to the client (simulates new)
  // Throws an exception if the object can't be allocated. (Memory allocation problem)
  void *Allocate(const char *label = 0);

  // Returns an object to the free list for the client (simulates delete)
  // Throws an exception if the the object can't be freed. (Invalid object)
  void Free(void *Object);

  // Calls the callback fn for each block still in use
  unsigned DumpMemoryInUse(DUMPCALLBACK fn) const;

  // Calls the callback fn for each block that is potentially corrupted
  unsigned ValidatePages(VALIDATECALLBACK fn) const;

  // Frees all empty pages (extra credit)
  unsigned FreeEmptyPages();

  // Returns true if FreeEmptyPages and alignments are implemented
  static bool ImplementedExtraCredit();

  // Testing/Debugging/Statistic methods
  void SetDebugState(bool State); // true=enable, false=disable
  const void *GetFreeList() const; // returns a pointer to the internal free list
  const void *GetPageList() const; // returns a pointer to the internal page list
  OAConfig GetConfig() const; // returns the configuration parameters
  OAStats GetStats() const; // returns the statistics for the allocator

  // Prevent copy construction and assignment
  ObjectAllocator(const ObjectAllocator &oa) = delete; //!< Do not implement!
  ObjectAllocator &operator=(const ObjectAllocator &oa) = delete; //!< Do not implement!

private:
  GenericObject *page_list;
  GenericObject *free_objects_list;

  size_t object_size;
  OAConfig config;
  size_t block_size;
  size_t page_size;

  OAStats stats;

  // top-level private methods

  /**
   * \brief Use the C++ native memory allocator to allocate an object
   *
   * \return Pointer to the object's location in memory
   */
  GenericObject *cpp_mem_manager_allocate(const char *label);

  /**
   * \brief Use the C++ native memory allocator to free an object from memory
   *
   * \param object Pointer to the object to free
   */
  void cpp_mem_manager_free(void *object);

  /**
   * \brief Use the custom object allocator to allocate an object in memory
   *
   * \return Pointer to the object's location in memory
   */
  GenericObject *custom_mem_manager_allocate(const char *label);

  /**
   * \brief Use the custom memory allocator to free an object from memory
   *
   * \param object Pointer to the object to free
   */
  void custom_mem_manager_free(void *object);

  // Object Management

  /**
   * \brief Links object in such a way that it is the front of the free object list
   *
   * \param object The object that will be inserted into the linked list
   */
  void object_push_front(GenericObject *object);

  /**
   * \brief Returns the first object in the free_objects_list
   *
   * \return The pointer to the object's location
   */
  GenericObject *object_pop_front();

  // Page Management

  /**
   * \brief Factory method for a page in memory.
   *
   * \return Pointer to allocated page
   */
  GenericObject *allocate_page();

  /**
   * \brief Updates `free_object_list` to include the pointers to the next available blocks. It also adds the page to
   * the `page_list`.
   *
   * \param page The page to add
   */
  void page_add(GenericObject *page);

  /**
   * \brief Returns the header size for the given info
   *
   * \param info The structure of the header
   * \return The size of the header
   */
  size_t get_header_size(OAConfig::HeaderBlockInfo info) const;

  /**
   * \brief Returns the left alignment size
   *
   * \return The size of the left alignment bytes
   */
  size_t calculate_left_alignment_size() const;

  /**
   * \brief Returns the inter alignment size
   *
   * \return The size of the inter alignment bytes
   */
  size_t calculate_inter_alignment_size() const;

  /**
   * \brief Returns the size of a block with the current config
   *
   * \return The size of a block in a page
   */
  size_t calculate_block_size() const;

  /**
   * \brief Returns the size of an individual page
   *
   * \return The size of the page
   */
  size_t calculate_page_size() const;
};

#endif
