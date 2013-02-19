all: icmp


icmp: icmp.c
	gcc -o icmp icmp.c

clean:
	@rm -f icmp

