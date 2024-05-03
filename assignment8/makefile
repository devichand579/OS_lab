all: process.c MMU.c master.c sched.c
	gcc master.c -o master -lm
	gcc sched.c -o scheduler -lm
	gcc MMU.c -o mmu -lm
	gcc process.c -o process -lm
	./master	

clean:
	rm process master scheduler mmu