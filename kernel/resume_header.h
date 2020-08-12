#ifndef _RESUME_HEADER_H_
#define _RESUME_HEADER_H_

#define MAXNAME 16

struct resumehdr {
	int memory_size;
	int code_size;
	int stack_size;
	int tracing;
	char name[MAXNAME];
};

#endif
