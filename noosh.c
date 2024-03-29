#include <unistd.h>
#include <libgen.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <linux/limits.h>
#include <bits/local_lim.h>

/*
  Color configuration
*/

/*
  color config struct
    username and cwd color
    user@host:/home/user$
*/
struct ColorConfig {
  int username_color;
  int cwd_color;
};

/*
  @brief reads config file
  @params filename: string containing filename of config
  @returns color config struct
*/
struct ColorConfig read_config(const char *filename) {
  struct ColorConfig config = {32, 35};

  char exePath[PATH_MAX];
  char dirPath[PATH_MAX];
  char configPath[PATH_MAX];

  // Get the executable's path
  ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
  if (len != -1) {
    exePath[len] = '\0'; // Null-terminate the result of readlink

    // Extract the directory
    strcpy(dirPath, dirname(exePath));

    // Construct the full path to the config file
    snprintf(configPath, sizeof(configPath), "%s/%s", dirPath, filename);
  } else {
    perror("Error finding executable path");
    return config;
  }

  FILE *file = fopen(configPath, "r");
  char line[128];

  if (!file) {
    perror("Error opening config file");
    return config;
  }

  while (fgets(line, sizeof(line), file)) {
    char *token = strtok(line, "=");
    if (token) {
      int value = atoi(strtok(NULL, "="));
      if (strcmp(token, "username_color") == 0) {
        config.username_color = value;
      } else if (strcmp(token, "cwd_color") == 0) {
        config.cwd_color = value;
      }
    }
  }

  fclose(file);
  return config;
}


/*
    @brief helper function that returns a string containing cwd
    @params args: list of args, not used
    @returns string containing cwd
*/
char* get_cwd(char ** args) {
  char *buf = malloc(sizeof(char) * PATH_MAX);
  if (!buf) {
    perror("noosh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  if (getcwd(buf, PATH_MAX) != NULL) {
    return buf;
  } else {
    perror("noosh");
    free(buf);
    exit(EXIT_FAILURE);
  }
}

/*
    function declarations for builtin shell commands:
*/
int noosh_cd(char ** args);
int noosh_pwd(char ** args);
int noosh_help(char ** args);
int noosh_exit(char ** args);

/*
    list of builtin commands, followed by their corresponding functions.
*/
char * builtin_str[] = {
  "cd",
  "pwd",
  "help",
  "exit"
};

int( * builtin_func[])(char ** ) = {
  &
  noosh_cd,
  &
  noosh_pwd,
  &
  noosh_help,
  &
  noosh_exit
};

int noosh_num_builtins() {
  return sizeof(builtin_str) / sizeof(char * );
}

/*
    builtin function implementations
*/

/*
    @brief builtin command: change director.
    @param args: list of args
        args[0] is  cd, args[1] is the directory
    @return always returns 1 to continue executing
*/
int noosh_cd(char ** args) {
  if (args[1] == NULL) {
    fprintf(stderr, "noosh: expected argument to \"cd\"\n");
  } else {
    if (chdir(args[1]) != 0) {
      perror("noosh");
    }
  }
  return 1;
}

/*
    @brief builtin command: print working directory
    @param args: list of args, not examined
    @return always returns 1 to continue executing
*/
int noosh_pwd(char ** args) {
  printf("%s\n", get_cwd(NULL));
  return 1;
}

/*
    @brief builtin command: print help info
    @param args: list of args, not examined
    @return always returns 1 to continue executing
*/
int noosh_help(char ** args) {
  int i;
  printf("noosh\n");
  printf("Type program names and arguments, and hit enter.\n");
  printf("The following are built in:\n");

  for (i = 0; i < noosh_num_builtins(); i++) {
    printf("  %s\n", builtin_str[i]);
  }

  printf("Use the man command for information on other programs.\n");
  return 1;
}

/*
    @brief builtin command: exit
    @param args: list of args, not examined
    @return always returns 0 to terminate execution
*/
int noosh_exit(char ** args) {
  return 0;
}

/*
    @brief launch a program and wait for it to terminate
    @param args: null terminated list of arguments
        args[0] is program, *args[1] is progrm args
    @return always return 1 to continue execution
*/
int noosh_launch(char ** args) {
  pid_t pid, wpid;
  int status;

  pid = fork();
  if (pid == 0) {
    // Child process
    if (execvp(args[0], args) == -1) {
      perror("noosh");
    }
    exit(EXIT_FAILURE);
  } else if (pid < 0) {
    // Error forking
    perror("noosh");
  } else {
    // Parent process
    do {
      wpid = waitpid(pid, & status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
  }

  return 1;
}

/*
    @brief execute shell programs
    @param args null terminated list of arguments
    @return 1 if the shell could continue running,
        0 if it should terminate
*/
int noosh_execute(char ** args) {
  int i;

  if (args[0] == NULL) {
    // An empty command was entered.
    return 1;
  }

  for (i = 0; i < noosh_num_builtins(); i++) {
    if (strcmp(args[0], builtin_str[i]) == 0) {
      return ( * builtin_func[i])(args);
    }
  }

  return noosh_launch(args);
}

# define NOOSH_RL_BUFSIZE 1024
/*
    @brief read a line of input from stdin
    @return the line from stdin
*/
char * noosh_read_line(void) {
  int bufsize = NOOSH_RL_BUFSIZE;
  int position = 0;
  char * buffer = malloc(sizeof(char) * bufsize);
  int c;

  if (!buffer) {
    fprintf(stderr, "noosh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  while (1) {
    // Read a character
    c = getchar();

    // If we hit EOF, replace it with a null character and return.
    if (c == EOF || c == '\n') {
      buffer[position] = '\0';
      return buffer;
    } else {
      buffer[position] = c;
    }
    position++;

    // If we have exceeded the buffer, reallocate.
    if (position >= bufsize) {
      bufsize += NOOSH_RL_BUFSIZE;
      buffer = realloc(buffer, bufsize);
      if (!buffer) {
        fprintf(stderr, "noosh: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }
  }
}

#define NOOSH_TOK_BUFSIZE 64
#define NOOSH_TOK_DELIM " \t\r\n\a"
/*
    @brief split a line into tokens
    @param line: the line
    @return null-terminated array of tokens
*/
char ** noosh_split_line(char * line) {
  int bufsize = NOOSH_TOK_BUFSIZE, position = 0;
  char ** tokens = malloc(bufsize * sizeof(char * ));
  char * token;

  if (!tokens) {
    fprintf(stderr, "noosh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  token = strtok(line, NOOSH_TOK_DELIM);
  while (token != NULL) {
    tokens[position] = token;
    position++;

    if (position >= bufsize) {
      bufsize += NOOSH_TOK_BUFSIZE;
      tokens = realloc(tokens, bufsize * sizeof(char * ));
      if (!tokens) {
        fprintf(stderr, "noosh: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }

    token = strtok(NULL, NOOSH_TOK_DELIM);
  }
  tokens[position] = NULL;
  return tokens;
}

/*
    @brief loop parsing and executing input
*/
void noosh_loop(void) {
  struct ColorConfig config = read_config("noosh_config.txt");

  char * line;
  char ** args;
  int status;
  char hostname[HOST_NAME_MAX];
  char *username;
  char *cwd;

  gethostname(hostname, HOST_NAME_MAX);
  username = getenv("USER");

  do {
    cwd = get_cwd(NULL);

    printf("\033[0;%dm%s@\033[0;%dm%s\033[0m:\033[0;%dm%s\033[0m$ ",
               config.username_color, username,
               config.username_color, hostname,
               config.cwd_color, cwd);

    line = noosh_read_line();
    args = noosh_split_line(line);
    status = noosh_execute(args);

    free(line);
    free(args);
  } while (status);
}

/*
    @brief main func entry point
    @param argc arg count
    @param argv arg vector
    @return status code
*/
int main(int argc, char ** argv) {
  // TODO: implement config files

  // Run command loop.
  noosh_loop();

  return EXIT_SUCCESS;
}