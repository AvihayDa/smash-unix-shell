# Smash — Small Shell

A Unix-like shell implemented in C++ as part of the Operating Systems course at the Technion.

The shell supports execution of built-in and external commands, job control, and signal handling, mimicking core behavior of a Linux shell.

## Main Features

- Built-in commands (e.g., `cd`, `pwd`, `jobs`, `fg`, `kill`, `quit`)
- Execution of external commands using `fork` and `exec`
- Job control with foreground and background processes
- Jobs list management with job IDs and process IDs
- Signal handling (e.g., Ctrl+C / SIGINT)
- Support for background execution using `&`
- Command parsing and error handling

## Build and Run

```bash
make
./smash
```

## Assignment Context

This project was developed for HW1 in the Operating Systems course at the Technion.

The original assignment description is included in:

```
OS_HW1_2026.pdf
```

## Technologies

- C++
- Linux
- System calls (fork, exec, waitpid, kill)
- Signals (SIGINT)
