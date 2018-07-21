#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#define SHELL_BUFFER_SIZE 		1024
#define SHELL_TOKEN_SIZE 		64


/* Path struct
 *
 * accessing by value
 * p.num_paths = total number of paths
 * p.name[i][] = path name
 *
 * where 0 <= i <= num_paths
 *
 */
struct path {
	int num_paths;
	char name[10][64];
};

typedef struct path Path;


/* Commands struct
 *
 * accessing by value
 * c.num_arguments = total number of arguments + command
 * c.command[i][] = command or argument
 *
 * where 0 <= i <= num_arguments
 *
 */
struct commands {
	int num_args;
	char* args[16];
	int or_flag;
	int pp_flag;
};

typedef struct commands Commands;


/* Fork struct
 *
 * accessing by value
 * p.pid = id value
 * p.status = status
 *
 */
struct fork {
	int pid;
	int status;
};

typedef struct fork Fork;



/* function prototypes */

void shell_loop(void);
void parse_abs_path(int start_up, Path *p);
void parse_commands(Commands *c);
void fork_process(Path *p, Commands *c, Fork *f);


/*
 * parse_abs_path, arguments: int start_up, Path *p
 *
 * int start_up: flag for printing the start up message (first line of path.txt)
 * Path *path: pointer to struct for path names
 *
 * parses ".sh360rc/path.txt"
 *
 * if start_up flag != 0, print start up message and set flag to 0
 *
 * clear out the buffer, and continue getting lines from path.txt
 * store each new line from buffer into Path struct
 *
 */

void parse_abs_path(int start_up, Path *path){
	char buffer[1024];
	FILE *fp;
	int index = 0;

	fp = fopen(".sh360rc", "r");
	fgets(buffer, 1024, fp);
	if(start_up != 0){
		fprintf(stdout, "%s\n", buffer);
		start_up = 0;
	}

	// clear out the buffer and clean up the path name before storing
	memset(&buffer[0], 0, sizeof(buffer));
	while(fgets(buffer, 1024, fp)){
		strcpy(path->name[index], strtok(buffer, "\n"));
		strcat(path->name[index], "/");
		memset(&buffer[0], 0, sizeof(buffer));
		index++;
	}

	path->num_paths = index - 1;
}


/*
 * parse_commands, arguments: Commands *c
 *
 * Commands *c: pointer to struct for commands
 *
 * parses line from stdin and stores it in the Command struct
 * if first argument is OR or PP, update struct flag
 *
 * store subsequent arguments in Commands struct in c.args[]
 *
 */

void parse_commands(Commands *c){

	// read line
	char* line = NULL;
	ssize_t buffer_size = 0;
	getline(&line, &buffer_size, stdin);
	c->num_args = 0;
	if( strlen(line) < 3){ return; }


	char* arg;
	arg = strtok(line, " \t\r\n\a");

	if( strcmp(arg, "OR") == 0 ){
		c->or_flag = 0;
	} else if( strcmp(arg, "PP") == 0 ){
		c->pp_flag = 0;
	} else {
		c->args[0] = arg;
		c->num_args = 1;
		c->or_flag = -1;
		c->pp_flag = -1;
	}

	// continue parsing line
	while(arg != NULL){
		arg = strtok(NULL, " \t\r\n\a");
		c->args[c->num_args] = arg;
		c->num_args++;

	}
	c->args[c->num_args-1] = 0;

	if( c->num_args > 8){
		fprintf(stdout, "You have submitted too many arguments.\n");
		return;
	}

}


/*
 * fork_process, arguments: Path *p, Commands *c, Fork *f
 *
 * Path *p: struct for the abs path name
 * Commands *c: struct for the commands from stdin
 * Fork *f: struct to store id and status var for fork
 *
 * checks if executable for command is any of the path names stored in *p
 * use *c to concatenate command to path
 * once correct abs path for executable is found, use execve
 * execve runs executable from args[0] in the child process
 * updates to pid, status variables are stored in *f
 *
 */

void fork_process(Path *p, Commands *c, Fork *f){
	char *envp[] = { 0 };
	struct stat file_stat;
	int path_index = 0;
	char command[16];
	char path_buffer[128];
	int fd;


	f->pid = fork();
	if( f->pid == 0){

		strcpy(command, c->args[0]);
		while( path_index <= p->num_paths ){

			strcpy(path_buffer, p->name[path_index]);
			strcat(path_buffer, command);
			c->args[0] = path_buffer;
			if( stat(c->args[0], &file_stat) == 0 ) break;
			path_index++;
		}

		// OR condition
		if(c->or_flag == 0){
			if( strcmp(c->args[c->num_args-3], "->") == 0){
				c->args[c->num_args-3] = c->args[c->num_args-2];
				c->args[c->num_args-2] = 0;
				c->num_args--;
			} else {
				fprintf(stderr, "wrong -> symbol.\n");
				return;
			}

			printf("printing to file: %s\n", c->args[c->num_args-2]);
			fd = open(c->args[c->num_args-2], O_CREAT|O_RDWR, S_IRUSR|S_IWUSR);
			if(fd == -1){
				fprintf(stderr, "Cannot open file for writing.\n");
				exit(-1);
			}
			dup2(fd, 1);
			dup2(fd, 2);

			execve(c->args[0], c->args, envp);
			if( f->pid == 0){
				close(fd);
				f->pid = 1;
				return;
			} else {
				waitpid(f->pid, &f->status, 0);
				return;
			}

		// PP condition
		} else if (c->pp_flag == 0){



		} else {

			execve(c->args[0], c->args, envp);

			if( f->pid == 0){
				f->pid = 1;
				return;
			} else {
				waitpid(f->pid, &f->status, 0);
				printf("pid: %d\n", f->pid);
			}
		}
	}
}


void shell_loop(void){
	int start_up = 1;

	Path p;
	p.num_paths = 0;
	Path *path = &p;

	Commands c;
	c.num_args = 0;
	Commands *commands = &c;

	Fork f;
	f.status = 1;
	Fork *fork = &f;

	parse_abs_path(start_up, path);


	do{

		fprintf(stdout, "$ ");
		fflush(stdout);

		parse_commands(commands);

		if( (strcmp(c.args[0], "exit") == 0 ) && c.args[1] == NULL){
			fprintf(stdout, "exiting ... \n" );
			exit(0);
		}

		fork_process(path, commands, fork);

	}

	while(waitpid(f.pid, &f.status, 0));
}


int main(int argc, char** argv){
	shell_loop();
	return(0);
}
