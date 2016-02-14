#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static const int s_max_threads = 4;
static const int s_stack_size = 0x400000;

struct green_thread {
	struct gt_context {
		uint64_t rsp;
		uint64_t r15;
		uint64_t r14;
		uint64_t r13;
		uint64_t r12;
		uint64_t rbx;
		uint64_t rbp;
	} context;
	enum {
		Unused,
		Running,
		Ready,
	} state;
};

struct green_thread gt_table[s_max_threads];
struct green_thread *gt_current;

void gt_init();
void gt_return(int ret);
void gt_switch(struct gt_context *old, struct gt_context *new);
bool gt_yield();
static void gt_stop();
int gt_go(void (*function)());

void gt_init()
{
	gt_current = &gt_table[0];
	gt_current->state = Running;
}

void __attribute__((noreturn))
gt_return(int exitValue)
{
	if (gt_current != &gt_table[0]) {
		gt_current->state = Unused;
		gt_yield();
		assert(!"reachable");
	}

	while (gt_yield()) {
		;
	}

	exit(exitValue);
}

bool gt_yield()
{
	struct green_thread *p;
	struct gt_context *old, *new;

	p = gt_current;
	while (p->state != Ready) {
		if (++p == &gt_table[s_max_threads]) {
			p = &gt_table[0];
		}
		if (p == gt_current) {
			return false;
		}
	}

	if (gt_current->state != Unused)
		gt_current->state = Ready;
	p->state = Running;
	old = &gt_current->context;
	new = &p->context;
	gt_current = p;

	gt_switch(old, new);

	return true;
}

static void gt_stop() {
	gt_return(0);
}

int gt_go(void (*function)())
{
	struct green_thread *p;
	for (p = &gt_table[0];; p++) {
		if (p == &gt_table[s_max_threads]) {
			return -1;
		}
		else if (p->state == Unused) {
			break;
		}
	}

	char *stack = malloc(s_stack_size);
	if (!stack) {
		return -1;
	}

	*(uint64_t *)&stack[s_stack_size -  8] = (uint64_t)gt_stop;
	*(uint64_t *)&stack[s_stack_size - 16] = (uint64_t)function;
	p->context.rsp = (uint64_t)&stack[s_stack_size - 16];
	p->state = Ready;

	return 0;
}

void do_work()
{
	static int x;

	int id = ++x;
	for (int i = 0; i < 10; i++) {
		printf("%d %d\n", id, i);
		gt_yield();
	}
}

int main()
{
	gt_init();
	gt_go(do_work);
	gt_go(do_work);
	gt_return(1);
}
