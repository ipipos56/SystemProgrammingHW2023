#include "userfs.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

// Constants defining block size and maximum file size in the file system.
enum {
  BLOCK_SIZE = 512,
  MAX_FILE_SIZE = 1024 * 1024 * 100,
};

/**
 * Global error code.
 * Set from any function on any error.
 */
// Global error code used across the file system for error handling.
static enum ufs_error_code ufs_error_code = UFS_ERR_NO_ERR;

// Block structure: Represents the basic unit of storage in UserFS.
struct block {

  char *memory; // Pointer to the allocated memory block.

  int occupied; // Number of bytes occupied in the block.
 
  struct block *next; // Pointer to the next block in the chain.

  struct block *prev; // Pointer to the prev block in the chain.
};

struct file {

  struct block *first_block; // Pointer to the first block in the file.

  struct block *last_block; // Pointer to the last block in the file.

  int refs; // Number of file descriptors that are using the file.

  bool is_ghost; // Whether the file was deleted, but has references.

  size_t size; // Size of the file in bytes.

  char *name; // File name.

  struct file *next; //  Pointer to the next file. NULL if it is the last file in the list.

  struct file *prev; // Pointer to the previous file. NULL if it is the first file in the list.
};

/** List of all files. */
static struct file *file_list = NULL;

struct filedesc {

  struct file *file; // Pointer to the file of the descriptor.

  int open_flags; // Code bitwise combination of flags, with which the file was open.

  size_t offset; // Current offset in the file (0 means first byte).
};

/**
 * An array of file descriptors. When a file descriptor is
 * created, its pointer drops here. When a file descriptor is
 * closed, its place in this array is set to NULL and can be
 * taken by next ufs_open() call.
 */
static struct filedesc **file_descriptors = NULL;
// static int file_descriptor_count = 0; RENAMED TO
static int file_descriptor_used = 0;
static int file_descriptor_capacity = 0;

enum ufs_error_code ufs_errno() { return ufs_error_code; }

void _ufs_unlink_file(struct file *file) {
  if (file->prev != NULL) {
    file->prev->next = file->next;
  }
  if (file->next != NULL) {
    file->next->prev = file->prev;
  }
  if (file_list == file) {
    file_list = file->next;
  }
}

void _ufs_free_file(struct file *file) {
  if (file->name != NULL) {
    free(file->name);
  }

  for (struct block *block = file->first_block; block != NULL;) {
    struct block *next = block->next;
    free(block->memory);
    free(block);
    block = next;
  }

  free(file);
}

// ufs_open: Opens a file with specified flags. Implement file opening modes here.
int ufs_open(const char *filename, int flags) {
  if (!(flags & UFS_READ_ONLY) && !(flags & UFS_WRITE_ONLY)) {
    flags |= UFS_READ_WRITE;
  }

  struct file *file = NULL;

  for (file = file_list; file != NULL; file = file->next) {
    if (!file->is_ghost && strcmp(file->name, filename) == 0) {
      // Found the file
      break;
    }
  }

  if (file == NULL) {
    if (!(flags & UFS_CREATE)) {
      // File not found and UFS_CREATE flag is not set
      ufs_error_code = UFS_ERR_NO_FILE;
      return -1;
    }

    // Create a new file
    file = malloc(sizeof(struct file));
    file->name = malloc(strlen(filename) + 1);
    strcpy(file->name, filename);
    file->is_ghost = false;
    file->refs = 0;
    file->size = 0;
    file->first_block = NULL;
    file->last_block = NULL;
    if (file_list == NULL) {
      // This is the first file in the list
      file->next = NULL;
      file->prev = NULL;
      file_list = file;
    } else {
      // This is not the first file in the list, prepend it to the beginning
      file->next = file_list;
      file->prev = NULL;
      file_list->prev = file;
      file_list = file;
    }
  }

  if (file_descriptor_capacity <= 0) {
    // There is no file descriptors yet, initialize them
    file_descriptors = malloc(sizeof(struct filedesc *) * 10);

    // Set all file descriptors to NULL
    for (int i = 0; i < 10; i++) {
      file_descriptors[i] = NULL;
    }

    file_descriptor_capacity = 10;
    file_descriptor_used = 0;
  }

  int idx = -1;

  if (file_descriptor_used < file_descriptor_capacity) {
    // Find the available one
    for (int i = 0; i < file_descriptor_capacity; i++) {
      if (file_descriptors[i] == NULL) {
        // Found an available file descriptor
        idx = i;
        break;
      }
    }

    if (idx < 0) {
      // Error: no available file descriptor, but it MUST be
      ufs_error_code = UFS_ERR_NOT_IMPLEMENTED;
      return -1;
    }
  } else {
    // No more available file descriptors, need to expand the array
    file_descriptors =
        realloc(file_descriptors,
                sizeof(struct filedesc *) * (file_descriptor_capacity * 2));

    // Set all new file descriptors to NULL
    for (int i = file_descriptor_capacity; i < file_descriptor_capacity * 2; i++) {
      file_descriptors[i] = NULL;
    }

    file_descriptor_capacity *= 2;
    idx = file_descriptor_used;
  }

  file_descriptors[idx] = malloc(sizeof(struct filedesc));
  file_descriptor_used++;
  file_descriptors[idx]->open_flags = flags;
  file_descriptors[idx]->offset = 0;
  file_descriptors[idx]->file = file;
  file->refs++;

  return idx + 1;  // Return the file descriptor number (i.e. idx + 1)
}

// ufs_write: Writes data to a file. Implement file growth mechanism here.
ssize_t ufs_write(int fd, const char *buf, size_t size) {
  int desc_idx = fd - 1;
  if (desc_idx < 0 || desc_idx >= file_descriptor_capacity) {
    // Invalid file descriptor
    ufs_error_code = UFS_ERR_NO_FILE;
    return -1;
  }

  struct filedesc *desc = file_descriptors[desc_idx];
  if (desc == NULL) {
    // Invalid file descriptor
    ufs_error_code = UFS_ERR_NO_FILE;
    return -1;
  }

  if (!(desc->open_flags & UFS_WRITE_ONLY) &&
      !(desc->open_flags & UFS_READ_WRITE)) {
    // File is opened for reading
    ufs_error_code = UFS_ERR_NO_PERMISSION;
    return -1;
  }

  if (size == 0) {
    // Nothing to write
    return 0;
  }

  if (desc->file->size + size > MAX_FILE_SIZE) {
    // Max file size exceeded
    ufs_error_code = UFS_ERR_NO_MEM;
    return -1;
  }

  struct block *block = desc->file->first_block;
  for (size_t i = 0; i < desc->offset / BLOCK_SIZE; i++) {
    block = block->next;
  }

  size_t bytes_written = 0;
  while (bytes_written < size) {
    size_t write_from = desc->offset % BLOCK_SIZE;

    if (block == NULL || block->occupied == BLOCK_SIZE) {
      // Create a new block
      block = malloc(sizeof(struct block));
      block->memory = malloc(BLOCK_SIZE);
      block->occupied = 0;
      if (desc->file->last_block == NULL) {
        // This is the first block
        assert(desc->file->first_block == NULL);
        desc->file->first_block = block;
        desc->file->last_block = block;
        block->prev = NULL;
        block->next = NULL;
      } else {
        // Append the block to the end of the file
        block->prev = desc->file->last_block;
        block->next = NULL;
        desc->file->last_block->next = block;
        desc->file->last_block = block;
      }
    }

    size_t bytes_to_write = size - bytes_written;
    if (bytes_to_write > BLOCK_SIZE - write_from) {
      bytes_to_write = BLOCK_SIZE - write_from;
    }

    memcpy(block->memory + write_from, buf + bytes_written,
           bytes_to_write);
    if ((int)(write_from + bytes_to_write) > block->occupied) {
      // Update the occupied size
      block->occupied = (int)(write_from + bytes_to_write);
    }
    bytes_written += bytes_to_write;
    desc->offset += bytes_to_write;
  }

  if (desc->offset > desc->file->size) {
    // Update the file size
    desc->file->size = desc->offset;
  }

  return (ssize_t)bytes_written;
}

// ufs_read: Reads data from a file. Ensure the file descriptor reads sequentially.
ssize_t ufs_read(int fd, char *buf, size_t size) {
  int desc_idx = fd - 1;
  if (desc_idx < 0 || desc_idx >= file_descriptor_capacity) {
    // Invalid file descriptor
    ufs_error_code = UFS_ERR_NO_FILE;
    return -1;
  }

  struct filedesc *desc = file_descriptors[desc_idx];
  if (desc == NULL) {
    // Invalid file descriptor
    ufs_error_code = UFS_ERR_NO_FILE;
    return -1;
  }

  if (!(desc->open_flags & UFS_READ_ONLY) &&
      !(desc->open_flags & UFS_READ_WRITE)) {
    // File is opened for writing
    ufs_error_code = UFS_ERR_NO_PERMISSION;
    return -1;
  }

  struct block *block = desc->file->first_block;
  for (size_t i = 0; i < desc->offset / BLOCK_SIZE; i++) {
    if (block == NULL) {
      // We've reached the end of the file
      return 0;
    }
    block = block->next;
  }

  if (desc->offset >= desc->file->size) {
    // We've reached the end of the file
    return 0;
  }

  ssize_t read_count = 0;
  while (block != NULL && read_count < (ssize_t)size) {
    int block_offset = (int)(desc->offset) % BLOCK_SIZE;

    if (block_offset - block->occupied >= 0) {
      // We've reached the end of the file
      return read_count;
    }

    size_t bytes_to_copy = block->occupied - block_offset;
    if (bytes_to_copy > size - read_count) {
      bytes_to_copy = size - read_count;
    }

    memcpy(buf + read_count, block->memory + block_offset, bytes_to_copy);
    read_count += (ssize_t)(bytes_to_copy);
    desc->offset += bytes_to_copy;
    block = block->next;
  }

  return read_count;
}

// ufs_close: Closes an open file. Handle the decrement of file reference count here.
int ufs_close(int fd) {
  int desc_idx = fd - 1;
  if (desc_idx < 0 || desc_idx >= file_descriptor_capacity) {
    // Invalid file descriptor
    ufs_error_code = UFS_ERR_NO_FILE;
    return -1;
  }

  struct filedesc *desc = file_descriptors[desc_idx];
  if (desc == NULL) {
    // Invalid file descriptor
    ufs_error_code = UFS_ERR_NO_FILE;
    return -1;
  }

  desc->file->refs--;
  if (desc->file->refs <= 0 && desc->file->is_ghost) {
    // No more file descriptors and no any references to the file, delete it
    _ufs_unlink_file(desc->file);
    _ufs_free_file(desc->file);
  }

  free(desc);
  file_descriptors[desc_idx] = NULL;
  file_descriptor_used--;

  return 0;
}

// ufs_delete: Deletes a file. Ensure proper memory deallocation to avoid leaks.
int ufs_delete(const char *filename) {
  struct file *file = NULL;
  for (file = file_list; file != NULL; file = file->next) {
    if (!file->is_ghost && strcmp(file->name, filename) == 0) {
      // Found the file
      break;
    }
  }

  if (file == NULL) {
    // File not found
    ufs_error_code = UFS_ERR_NO_FILE;
    return -1;
  }

  if (file->refs <= 0) {
    _ufs_unlink_file(file);
    _ufs_free_file(file);
  } else {
      file->is_ghost = true;
  }

  return 0;
}

// ufs_resize: Resizes a file. Implement resizing logic and memory management here.
int ufs_resize(int fd, size_t new_size) {
  int desc_idx = fd - 1;
  if (desc_idx < 0 || desc_idx >= file_descriptor_capacity) {
    // Invalid file descriptor
    ufs_error_code = UFS_ERR_NO_FILE;
    return -1;
  }

  struct filedesc *desc = file_descriptors[desc_idx];
  if (desc == NULL) {
    // Invalid file descriptor
    ufs_error_code = UFS_ERR_NO_FILE;
    return -1;
  }

  if (new_size == desc->file->size) {
    // Nothing to do
    return 0;
  } else if (new_size < desc->file->size) {
    size_t new_block_count = (new_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    size_t old_block_count = (desc->file->size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    int new_last_block_occupied = (int)((new_size) % BLOCK_SIZE);
    if (new_last_block_occupied == 0) {
      new_last_block_occupied = BLOCK_SIZE;
    }
    struct block *block = desc->file->last_block;
    for (size_t i = 0; i < old_block_count - new_block_count; i++) {
      struct block *prev_block = block->prev;

      if (prev_block != NULL) {
        prev_block->next = NULL;
      }

      free(block->memory);
      free(block);

      block = prev_block;
    }

    if (block == NULL) {
      assert(new_size == 0);
      assert(new_block_count == 0);
      desc->file->first_block = NULL;
      desc->file->last_block = NULL;
      desc->file->size = 0;
    } else {
      block->occupied = new_last_block_occupied;
      desc->file->last_block = block;
      desc->file->size = new_size;
    }

    for (int i = 0; i < file_descriptor_capacity; i++) {
      if (file_descriptors[i] != NULL && file_descriptors[i]->file == desc->file && file_descriptors[i]->offset > new_size) {
        file_descriptors[i]->offset = new_size;
      }
    }

    return 0;
  } else if (new_size > desc->file->size) {
    if (new_size > MAX_FILE_SIZE) {
      // File is too big
      ufs_error_code = UFS_ERR_NO_MEM;
      return -1;
    }

    size_t new_block_count = (new_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    size_t old_block_count = (desc->file->size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    int new_last_block_occupied = (int)((new_size) % BLOCK_SIZE);
    if (new_last_block_occupied == 0) {
      new_last_block_occupied = BLOCK_SIZE;
    }
    for (size_t i = 0; i < new_block_count - old_block_count; i++) {
      struct block *block = malloc(sizeof(struct block));
      block->memory = calloc(BLOCK_SIZE, 1);

      block->next = NULL;
      block->prev = desc->file->last_block;
      if (desc->file->last_block != NULL) {
        desc->file->last_block->next = block;
        desc->file->last_block->occupied = BLOCK_SIZE;
      }
      desc->file->last_block = block;
      if (desc->file->first_block == NULL) {
        desc->file->first_block = block;
      }
    }

    desc->file->last_block->occupied = new_last_block_occupied;
    desc->file->size = new_size;

    return 0;
  }

  // should be impossible
  ufs_error_code = UFS_ERR_NOT_IMPLEMENTED;
  return -1;
}

// ufs_destroy: Cleans up resources. Ensure all memory is freed to avoid leaks.
void ufs_destroy(void) {
  if (file_descriptor_capacity > 0 || file_descriptors != NULL) {
    for (int i = 0; i < file_descriptor_capacity; i++) {
      if (file_descriptors[i] != NULL) {
        free(file_descriptors[i]);
      }
    }
    free(file_descriptors);
    file_descriptors = NULL;
  }
  file_descriptor_capacity = 0;
  file_descriptor_used = 0;

  for (struct file *file = file_list; file != NULL;) {
    struct file *next = file->next;
    _ufs_free_file(file);
    file = next;
  }
  file_list = NULL;
}