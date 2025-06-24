# Custom Unix Shell  
A minimal Unix-like shell implemented in C, designed as a student project.  
  
## Features  
- Basic command execution  
- Multi-stage pipelines (`pipes "cmd1" "cmd2" "cmd3"`)  
- Input (`<`) and output (`>`) redirection  
- Background execution (`&`)  
- Support for built-in commands  
  
**Example usage**  
pipes "cat /etc/passwd" "cut -d: -f7" "sort" "uniq -c"  
