#include <stdio.h>
void main(int argc, char **argv){
	if (argc != 3){
		printf("usage: ascii_sanitizer infile outfile\n");
		return;
	}
	FILE *i = fopen(argv[1],"rb");
	if (!i){
		printf("file %s not found\n",argv[1]);
		return;
	}
	FILE *o = fopen(argv[2],"wb");
	if (!o){
		printf("file %s not found\n",argv[2]);
		return;
	}
	fseek(i,0,SEEK_END);
	size_t s = ftell(i);
	fseek(i,0,SEEK_SET);
	char *c = malloc(s);
	fread(c,s,1,i);
	fclose(i);
	for (size_t j = 0; j < s; j++){
		if ((c[j] >= ' ' && c[j] <= '~') || (c[j] == ' ' || c[j] == '\t' || c[j] == '\n')) fputc(c[j],o);
	}
	fclose(o);
}