#include<iostream>
#include<sys/wait.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<fcntl.h>
#include<dirent.h>

using namespace std;

// Some char arrays which will be used frequently in the program
char d_exit[] = "exit";
char d_pwd[] = "pwd";
char d_clear[] = "clear";
char d_ls[] = "ls";
char d_cd[] = "cd";
char d_environ[] = "environ";
char d_setenv[] = "setenv";
char d_unsetenv[] = "unsetenv";


// All functions prototype. Their code is after main function
int piping(char arr[], int desc, char** env);
void tokenize(char arr[], char** &substr, int& num, char character);
void trim(char arr[]);
void execute_command( char input[], char** env );

// function prototypes for built in commands
int is_builtin( char arr[] );
void fn_pwd();
void fn_ls( char path[] );
void fn_cd(char path[] );
void fn_environ(char** arr);
void fn_setenv(char var[], char val[] );
void fn_unsetenv(char var[] );
void execute_builtin( int cmd_num, int arg_num, char arg1[], char arg2[], char** arg3 );
void set_parent_en_var();

// main function
int main(int argc, char **argv, char** env)
{
	// variables for shell path and user info
	char user_name[50];
	char host_name[50];
	char current_dir[100];

	// shell setups	
	cout << "\x1B[2J\x1B[H";		// clears screen
	getcwd(current_dir, 50);
	setenv( "SHELL", current_dir, 1);

	// variables to get input from user
	char input[100];
	int len_str = 0;

	while( 1 )
	{		
		// getting user name, host name and path of current working directory
		getlogin_r(user_name, 50);
		gethostname(host_name, 50);
		getcwd(current_dir, 100);

		
		// displaying path and name. Also getting command input from the user
		cout << user_name << "@" << host_name << " " << current_dir << " > ";
		cin.getline( input, 100, '\n');
		trim(input);
		len_str = strlen( input );

		// exit implementation in start
		if( strstr(input, "exit") != NULL)
			exit(0);	

		// if there is an ampersand at the end of the command
		if( input[len_str-1] == '&')
		{
			// remove the ampresend from the command
			input[len_str-1] = input[len_str];
			trim(input);

			// execute the command after fork
			pid_t f = fork();
			if( f == 0 )
			{
				// setting parent environment variable
				set_parent_en_var();

				execute_command( input, env );	
				exit(0);
			}
		}
		// if there is no ampersand then execute the command normally
		else
			execute_command( input, env );
	}

	exit(0); 	
}


// Execute Command Function
// this function takes two arguments: 1) input/command and 2) env array of environment variables
// and executes the command
void execute_command( char input[], char** env )
{
	char** substr = nullptr;
	int num;

	// counting how many >, < and | symbols are in the command
	int len_str = strlen(input);
	int bar = 0;
	int greater = 0;
	int less = 0;
	for( int a=0; a<len_str; a++ )
	{
		if( input[a] == '|')
			bar++;
		if( input[a] == '>')
			greater++;
		if( input[a] == '<')
			less++;
	}

	// if the command has > but no < symbol
	if( greater > 0 && less == 0)
	{
		tokenize(input, substr, num, '>');

		// if the command has > symbol and also | symbol
		if( bar >  0)
		{
			// opening file specified after > symbol
			int file = open( substr[1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
			// executing command specified before > symbol
			if( file > 0)
				piping(substr[0], file, env);
			else
				cout << "Error file opening" << endl;
		}

		// if the command has only > symbol
		else
		{
			// opening file after >
			int file = open( substr[1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
			if( file > 0)
			{
				pid_t f = fork();
				if( f == 0 )
				{
					// setting parent environment variable
					set_parent_en_var();

					// executing the command specified before > symbol
					dup2(file, 1);
					char** sub;		int num2;
					tokenize( substr[0], sub, num2, ' ');
					int n = is_builtin( sub[0] );

					// if the function is not built in then call exec
					if( n == -1 )
						execvp( sub[0], sub);
					else
					{
						// if the function is built in, set and pass all the necessary arguments and execute it
						char* arg1 = nullptr;
						char* arg2 = nullptr;
						char** arg3 = env;
						if( num2 > 1)
							arg1 = sub[1];
						if( num2 > 2)
							arg2 = sub[2];
						execute_builtin( n, 0, arg1, arg2, arg3 );
						exit(0);
					}
				}
				else
					wait(NULL);
			}
			else
				cout << "Error file opening" << endl;
		}
	}
		
	// if the command has < symbol but no > symbol
	else if( greater == 0 && less > 0)
	{
		tokenize(input, substr, num, '<');

		// if the command has < symbol and also | symbol
		if( bar >  0)
		{
			int fds[2];
			pipe( fds );
			// execute the command specified after < symbol
			piping(substr[1],fds[1], env );
			// execute the command specified before < symbol. Data will used from the pipe
			pid_t f = fork();
			if( f == 0)
			{
				// setting parent environment variable
				set_parent_en_var();

				close( fds[1]);
				dup2(fds[0], 0);
				char** sub;
				int num2;
				tokenize( substr[0], sub, num2, ' ');
				int n = is_builtin( sub[0] );
				// if the command is not built in, call the exec
				if( n == -1 )
					execvp( sub[0], sub);
				else
				{
					// if the command is bult in, set the arguments and execute the command
					char* arg1 = nullptr;
					char* arg2 = nullptr;
					char** arg3 = env;
					if( num2 > 1)
						arg1 = sub[1];
					if( num2 > 2)
						arg2 = sub[2];
					execute_builtin( n, 0, arg1, arg2, arg3 );
					exit(0);
				}
			}
			else if( f > 0)
			{
				close( fds[1] );
				wait(NULL);
			}
		}

		// if the command has only < symbol
		else
		{
			// open the file specified after < symbol
			int file = open( substr[1], O_RDONLY);
			if( file > 0)
			{
				pid_t f = fork();
				if( f == 0)
				{
					// setting parent environment variable
					set_parent_en_var();

					dup2(file, 0);
					char** sub;				int num2;
					tokenize( substr[0], sub, num2, ' ');
					int n = is_builtin( sub[0] );
					// if the command is not built in, call exec
					if( n == -1 )
						execvp( sub[0], sub);
					else
					{
						// if the command is built in, execute the command after setting the required arguments
						char* arg1 = nullptr;
						char* arg2 = nullptr;
						char** arg3 = env;
						if( num2 > 1)
							arg1 = sub[1];
						if( num2 > 2)
							arg2 = sub[2];
						execute_builtin( n, 0, arg1, arg2, arg3 );
						exit(0);
					}
				}
				else if( f > 0)
					wait(NULL);
			}
			else
				cout << "Error file opening" << endl;
		}
	}
	
	// if the command has both < and > symbols
	else if( greater > 0 && less > 0)
	{
		// if the command has | symbol along with < and > symbol
		if( bar >  0)
		{
			// split the command first on the basis of > symbol
			tokenize(input, substr, num, '>');
			// open the file specified after > symbol
			int file = open( substr[1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
			char** sub;
			int num2;
			// again split the command on the basis of < symbol this time
			tokenize(substr[0], sub, num2, '<');
			int fds[2];
			pipe( fds );
			// execute the inner command between < and > symbols and save its result in a pipe
			piping( sub[1], fds[1], env );
			// execute the command specified before < symbol and send its data to the specified after > symbol
			pid_t f = fork();
			if( f == 0)
			{
				// setting parent environment variable
				set_parent_en_var();

				close( fds[1]);
				dup2(fds[0], 0);
				dup2( file, 1);
				char** sub2;
				tokenize( sub[0], sub2, num2, ' ');
				int n = is_builtin( sub2[0] );
				// if the command is not built in, call exec
				if( n == -1 )
					execvp( sub2[0], sub2);
				else
				{
					// if the command is built in, call the command function after setting some arguments
					char* arg1 = nullptr;
					char* arg2 = nullptr;
					char** arg3 = env;
					if( num2 > 1)
						arg1 = sub2[1];
					if( num2 > 2)
						arg2 = sub2[2];
					execute_builtin( n, 0, arg1, arg2, arg3 );
					exit(0);
				}
			}
			else if( f > 0)
			{
				close( fds[1] );
				wait(NULL);
			}
		}

		// if the command contains only > and < symbols
		else
		{	
			// first split the command on the basis of the > symbol
			tokenize(input, substr, num, '>');
			trim( substr[1] );
			// open the file specified after > symbol
			int file = open( substr[1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
			char** sub;
			int num2;
			// now again split the command on the basis of < symbol
			tokenize(substr[0], sub, num2, '<');
			// open the file specified in the middle of < and > symbols
			int file2 = open( sub[1], O_RDONLY);
			// execute the command specified before < symbol
			pid_t f = fork();
			if( f == 0)
			{
				// setting parent environment variable
				set_parent_en_var();

				dup2(file2,0);
				dup2( file, 1);
				char** sub2;
				tokenize( sub[0], sub2, num2, ' ');
				int n = is_builtin( sub2[0] );
				// if the command is not built in, call exec
				if( n == -1 )
					execvp( sub2[0], sub2);
				// if the command is built in, execute the command after setting and passing some arguments
				else
				{
					char* arg1 = nullptr;
					char* arg2 = nullptr;
					char** arg3 = env;
					if( num2 > 1)
						arg1 = sub2[1];
					if( num2 > 2)
						arg2 = sub2[2];
					execute_builtin( n, 0, arg1, arg2, arg3 );
					exit(0);
				}
			}
			else if( f > 0)
				wait(NULL);
		}
	}

	// if the command does not contain any < or > symbols
	else
	{
		// if the command contains bars
		if( bar > 0)
			piping( input, -1, env);
		
		// if the command is simple, without any <, > or | symbols
		else
		{
			tokenize( input, substr, num, ' ');
			if( strcmp(substr[0], d_exit) == 0)
				exit(0);
			int n = is_builtin( substr[0] );
			// if the command is not built in
			if( n == -1 )
			{
				// last point of part 1 implementation
				for( int i=0; i<num; i++)
					cout << substr[i] << endl;
				cout << endl;


				pid_t f = fork();
				if( f == 0 )
				{
					// setting parent environment variable
					set_parent_en_var();

					execvp( substr[0], substr);
				}
				else if( f > 0 )
					waitpid(f, NULL, NULL);
			}
			// if the command is built in
			else
			{
				char* arg1 = nullptr;
				char* arg2 = nullptr;
				char** arg3 = env;
				if( num > 1)
					arg1 = substr[1];
				if( num > 2)
					arg2 = substr[2];
				execute_builtin( n, 0, arg1, arg2, arg3 );	
			}
		}
	}
}


// Is Builtin Function
// this function determines if the command specified is built in or not
// if the command is built in then its number is returned
int is_builtin( char arr[] )
{
	if( strcmp( arr, d_exit) == 0)
		return 1;
	else if( strcmp( arr, d_pwd) == 0)
		return 2;
	else if( strcmp( arr, d_clear) == 0)
		return 3;
	else if( strcmp( arr, d_ls) == 0)
		return 4;
	else if( strcmp( arr, d_cd) == 0)
		return 5;
	else if( strcmp( arr, d_environ) == 0)
		return 6;
	else if( strcmp( arr, d_setenv) == 0)
		return 7;
	else if( strcmp( arr, d_unsetenv) == 0)
		return 8;
	return -1;
}

// Fn_Pwd function
// this function prints the current working directory on the screen
void fn_pwd()
{
	char dir_path[50];
	getcwd(dir_path, 50);
	cout << dir_path << endl;
}

// Fn_Ls function
// this function prints the contents of the specified directory
void fn_ls( char path[] )
{
	struct dirent **list;
    int num =  scandir( path, &list, NULL, alphasort);
	if( num > 0)
	{
		for( int i=0; i<num; i++)
			cout << list[i]->d_name << endl;
	}
	else
		cout << "ls command got can error" << endl;
}

// Fn_Cd function
// this function changes the directory
void fn_cd(char path[] )
{
	// if directory is specified
	if( path != NULL)
		chdir(path);
	// if directory is not specified
	else
	{
		char path_[] = "/home/";
		chdir(path_);
	}
}


// Fn_Environ function
// this function list all the environment variables on the screen
void fn_environ(char** arr)
{
	int count = 0;
	char* var;
  	for (char** en_var = arr; *en_var != 0; en_var++)
  	{
		  count++;
    	var = *en_var;
    	cout << var << endl;  
 	}
}

// Fn_Setenv function
// this function sets the value of the environment variable specified
void fn_setenv(char var[], char val[] )
{
	char* p = getenv(var);
	// if the environment variable is already set
	if( p != NULL )
		cout << "Environment Variable " << var << " has already been defined" << endl;
	// if the environment variable was not set
	else
	{
		// if a value is specified for environment varaible
		if( val != NULL)
			setenv( var, val, 1);
		// if a value was not specified for environment varaible
		else
			setenv( var, " ", 1);
	}
}

// Fn_UnsetEnv function
// this function is used to unset the value of an environment variable
void fn_unsetenv(char var[] )
{
	char* p = getenv(var);
	// if the environment variable does not have a value set
	if( p == NULL )
		cout << "Environment Variable " << var << " has not been defined" << endl;
	else
		unsetenv( var);
}

// Execute Builtin function
// this function executes the commands depanding upon cmd_num which were implemented
void execute_builtin( int cmd_num, int arg_num, char arg1[], char arg2[], char** arg3 )
{
	if( cmd_num == 1)
		exit(0);
	else if( cmd_num == 2 )
		fn_pwd();
	else if( cmd_num == 3 )
		cout << "\x1B[2J\x1B[H";		// clears screen
	else if( cmd_num == 4 )
		fn_ls( arg1 );
	else if( cmd_num == 5 )
		fn_cd( arg1 );
	else if( cmd_num == 6 )
		fn_environ( arg3 );
	else if( cmd_num == 7 )
		fn_setenv( arg1, arg2);
	else if( cmd_num == 8 )
		fn_unsetenv( arg1 );
}

// Set Parent En Var function
// this function is used to set the parent environment variable as path
void set_parent_en_var()
{
	char current_dir[100];
	getcwd(current_dir, 100);
	setenv( "PARENT", current_dir, 1);
}

// Piping function
// this function is used to handle all commands which involve | symbol
int piping(char arr[], int desc, char** env)
{
	// finding how many parts the piping command has
	char** sub;
	int splits = 0;
	// splitting command on the basis of | symbol
	tokenize( arr, sub, splits, '|');

	// creating pipes
	int** fd = new int* [splits+1];
	for( int i=0; i<splits+1; i++)
		fd[i] = new int[2];
	for( int i=0; i<splits+1; i++)
		pipe( fd[i] );
				
	char** substr;
	int num;
	for( int k=0; k<splits; k++)
	{
		// splitting command on the basis of space
		tokenize(sub[k], substr, num, ' ');

		// first substring the piping string
		if( k == 0)
		{
			int check = 1;
			// if the value of descriptor is greater than zero, it means some output point was provided
			if( desc > 0 )
			{
				// check if the first substring is a input file or not
				// if the file opens then the first substring is a input file
				int file = open( substr[0], O_RDONLY);
				if( file > 0 )
				{
					pid_t f = fork();
					if( f == 0 )
					{
						// setting parent environment variable
						set_parent_en_var();

						close( fd[k][0] );
						int r = 0;
						// read and send the contents of the file to pipe
						char w[1];
						while( (r = read(file, w, 1 )) > 0 )
							write( fd[k][1], w, 1);
						exit(0);
					}
					else if ( f > 0)
						wait(NULL);
				}
				else
					check = 0;
			}

			// the first substring was not a file
			if( desc < 0 || (desc > 0 && check == 0) )
			{
				// create a child and execute the command
				pid_t f = fork();
				if( f == 0 )
				{
					// setting parent environment variable
					set_parent_en_var();

					close( fd[k][0] );
					dup2( fd[k][1], 1);	
					int n = is_builtin( substr[0] );
					// if the command is not built in the call exec
					if( n == -1 )
						execvp( substr[0], substr);
					// if the command is built in then execute it after setting the required arguments
					else
					{
						char* arg1 = nullptr;
						char* arg2 = nullptr;
						char** arg3 = env;
						if( num > 1)
							arg1 = substr[1];
						if( num > 2)
							arg2 = substr[2];
						execute_builtin( n, 0, arg1, arg2, arg3 );
						exit(0);
					}
				}
				else if ( f > 0)
					wait(NULL);
			}
		}

		// last string in the piping
		else if( k == (splits - 1) )
		{
			pid_t f = fork();
			if( f == 0)
			{
				// setting parent environment variable
				set_parent_en_var();

				close( fd[k-1][1] );
				dup2( fd[k-1][0], 0);

				// if some output point was provided then send the data to that output point
				if( desc > 0)
					dup2( desc, 1 );
				int n = is_builtin( substr[0] );
				// if the command is not built in then call exec
				if( n == -1 )
					execvp( substr[0], substr);
				// if the command is built in then execute the built in command
				else
				{
					char* arg1 = nullptr;
					char* arg2 = nullptr;
					char** arg3 = env;
					if( num > 1)
						arg1 = substr[1];
					if( num > 2)
						arg2 = substr[2];
					execute_builtin( n, 0, arg1, arg2, arg3 );
					exit(0);
				}
			}
			else if( f > 0 )
			{
				close( fd[k-1][1] );
				wait(NULL);
			}
		}

		// intermediate substring in the piping string
		else
		{
			pid_t f = fork();
			if( f == 0)
			{
				// setting parent environment variable
				set_parent_en_var();

				close( fd[k-1][1] );
				close( fd[k][0] );
				dup2( fd[k-1][0], 0);
				dup2( fd[k][1], 1);
				int n = is_builtin( substr[0] );
				// if the command is not built in then call exec
				if( n == -1 )
					execvp( substr[0], substr);
				// if the command is built in then execute the built in command after setting some arguments
				else
				{
					char* arg1 = nullptr;
					char* arg2 = nullptr;
					char** arg3 = env;
					if( num > 1)
						arg1 = substr[1];
					if( num > 2)
						arg2 = substr[2];
					execute_builtin( n, 0, arg1, arg2, arg3 );
					exit(0);
				}
			}
			else if( f > 0)
			{
				close( fd[k-1][1] );
				wait(NULL);
			}
		}
	}	
	return 1;
}


// Tokenize function
// this function is used to tokenize a string based on the character specified
void tokenize(char arr[], char** &substr, int& num, char character)
{
	// remove the spaces from start and end of the string
	trim( arr );
	
	// save the size of string
	int size =  strlen( arr );

	// count the number of character in the string
	int ch = 0;
	for( int i=0; i<size; i++ )
	{
		if( arr[i] == character )
			ch++;
	}
	
	// initialize the array for tokens, tokens are 1 time more than spaces
	int tokens = ch + 1;
	num = tokens;
	substr = new char* [ tokens ];
	for( int j=0; j<tokens; j++ )
		substr[j] = new char[100];
	
	// save the tokens in substr
	int t = 0;
	int index = 0;
	for( int k=0; k<size; k++)
	{
		if( arr[k] != character )
		{
			substr[t][index] = arr[k];
			index++;
		}
		else
		{
			substr[t][index] = '\0';
			index = 0;
			t++;
		}
	}

	// call trim on all subparts to remove spaces
	for( int i=0; i<num; i++)
		trim( substr[i] );
}


// Trim function
// this function is used to remove extra spaces from the begining, middle and end of the string
void trim(char arr[])
{
	int size = strlen( arr );
	if( size > 0)
	{
		// removing spaces that were in the start of the string
		while( arr[0] == ' ')
		{
			size = strlen( arr );
			for(int i=0; i<size; i++)
				arr[i] = arr[i+1];	
		}

		// removing spaces that were in the end of the string
		size = strlen( arr );
		while( arr[ size - 1] == ' ')
		{
			arr[ size - 1 ] = arr[size];
			size = strlen( arr );
		}

		// removing two or more consective spaces
		size = strlen( arr );
		int p = 0,	// p = no. of position to move back
			x = 0;	// i + 1
		for( int i=0; i<size; i++)
		{
			// if the character was space
			if( arr[i] == ' ' )
			{
				p = 0;
				x = i + 1;
				while( arr[x] == ' ' )
				{
					p++;
					x++;
				}
				if( p > 0)
				{
					for(int j = i+1; x<=size; j++, x++)
						arr[j] = arr[x];
					size = strlen( arr );
				}				
			}
		}
	}
}
