#ifndef READER_H
#define READER_H

#define READER_BUFFER_SIZE 1024

struct reader
{
  int fd;
  char *cur;
  char *end;
  char buf[READER_BUFFER_SIZE];
};

// Initialize the reader.
void reader_init(struct reader *rd);

// Advance the read pointer, refilling the buffer if necessary.
int reader_next(struct reader *rd);

// Advance the read pointer. If the buffer would need to be refilled, move the
// block starting from `start` and extending to the read pointer to the start of
// the buffer, and continue filling from there. Returns a pointer to the start
// of the block.
char *reader_next_preserve(struct reader *rd, char *start);

#endif
