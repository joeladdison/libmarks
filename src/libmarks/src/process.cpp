#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <unistd.h>
#include <sys/wait.h>

#include "process.hpp"

/* Public */
Process::Process(std::vector<std::string> argv):
    input(NULL), output(NULL), error(NULL), finished(false),
    abnormalExit(false), signalled(false)
{
    // Create pipes
    if (pipe(fdIn) != 0 || pipe(fdOut) != 0 || pipe(fdErr) != 0) {
        std::cerr << "Pipe failed.\n";
        // exit(1);
    }

    // Fork
    childPid = fork();

    if (childPid < 0) {
        std::cerr << "Fork failed.\n";
        // exit(1);
    } else if (childPid == 0) {
        // Child process.
        setup_child(argv);
    } else {
        // Parent process.
        setup_parent();
    }
}

Process::~Process()
{
    if (!finished) {
        // TODO: Should this kill the process, or just wait?
        perform_wait();
    }
}

bool Process::send(const std::string& message)
{
    if (fprintf(input, "%s", message.c_str()) != message.length()) {
        // Sending failed.
        return false;
    }

    // Full message was sent.
    return true;
}

bool Process::send_file(char *filePath)
{
    std::ifstream input (filePath);

    // Write the contents of the file to the program.
    return false;
}

bool Process::expect_stdout(const std::string& expected)
{
    return expect(expected, output);
}

bool Process::expect_stderr(const std::string& expected)
{
    return expect(expected, error);
}

bool Process::expect_stdout_file(char *filePath)
{
    return expect_file(filePath, output);
}

bool Process::expect_stderr_file(char *filePath)
{
    return expect_file(filePath, error);
}

std::string Process::readline_stdout()
{
    return readline(output);
}

std::string Process::readline_stderr()
{
    return readline(error);
}

void Process::print_stdout()
{
    print_stream(output);
}

void Process::print_stderr()
{
    print_stream(error);
}

bool Process::assert_exit_status(int expected)
{
    if (!finished)
        perform_wait();
    return !abnormalExit && exitStatus == expected;
}

bool Process::assert_signalled(bool expected)
{
    if (!finished)
        perform_wait();
    return signalled == expected;
}

bool Process::assert_signal(int expected)
{
    if (!finished)
        perform_wait();
    return signalled && signalNum == expected;
}

int Process::get_exit_status()
{
    return exitStatus;
}

bool Process::get_abnormal_exit()
{
    return abnormalExit;
}

bool Process::get_signalled()
{
    return signalled;
}

int Process::get_signal()
{
    return signalNum;
}

void Process::send_kill()
{
    kill(childPid, SIGTERM);
    perform_wait();
}

/* Private */
void Process::setup_parent()
{
    if (close(fdIn[READ]) != 0 || close(fdOut[WRITE]) != 0 ||
            close(fdErr[WRITE]) != 0) {
        std::cerr << "Failed to close child pipes.\n";
        // exit(2);
    }

    input = fdopen(fdIn[WRITE], "w"); // Input from parent to child.
    output = fdopen(fdOut[READ], "r"); // stdout from child to parent.
    error = fdopen(fdErr[READ], "r"); // stderr from child to parent.

    if (input == NULL || output == NULL || error == NULL) {
        std::cerr << "Failed to open pipes as files.\n";
        // exit(2);
    }
}

void Process::setup_child(std::vector<std::string> argv)
{
    // TODO: Close on exec
    if (close(fdIn[WRITE]) == -1 ||
            dup2(fdIn[READ], STDIN_FILENO) == -1 ||
            close(fdIn[READ]) == -1) {
        std::cerr << "Child failed to initialise stdin.\n";
        exit(-1);
    }

    if (close(fdOut[READ]) == -1 ||
            dup2(fdOut[WRITE], STDOUT_FILENO) == -1 ||
            close(fdOut[WRITE]) == -1) {
        std::cerr << "Child failed to initialise stdout.\n";
        exit(-1);
    }

    if (close(fdErr[READ]) == -1 ||
            dup2(fdErr[WRITE], STDERR_FILENO) == -1 ||
            close(fdErr[WRITE]) == -1) {
        std::cerr << "Child failed to initialise stderr.\n";
        exit(-1);
    }

    // Execute the program.
    char **args = create_args(argv);
    execvp(args[0], args);
    perror("exec failed");
    delete_args(args, argv.size());

    // Exec failed if program reaches this point.
    std::cerr << "Child failed to exec.\n";
    exit(-1);
}

char **Process::create_args(std::vector<std::string> argv)
{
    char **args = new char*[argv.size()];
    for (int i = 0; i < argv.size(); ++i) {
        args[i] = new char[argv[i].length() + 1];
        argv[i].copy(args[i], argv[i].length());
        args[i][argv[i].length()] = '\0';
    }

    // argv array for exec* needs to be null terminated.
    args[argv.size()] = NULL;

    return args;
}

void Process::delete_args(char **args, size_t length)
{
    for (int i = 0; i < length; ++i) {
        delete [] args[i];
    }
    delete [] args;
}

bool Process::expect_file(char *filePath, FILE *stream)
{
    std::ifstream expected (filePath);

    // Char by char, check expected output against received output.
    while (expected.good() && !feof(output) && (expected.get() == fgetc(stream)));

    // If output was same as expected, then both should be at end of file.
    if (expected.eof() && feof(output))
        return true;

    return false;
}

bool Process::expect(const std::string& expected, FILE *stream)
{
    if (expected.length() == 0) {
        // TODO: Should this raise an exception?
        return false;
    }

    for (int i = 0; i < expected.length(); ++i) {
        if (expected[i] != (char) fgetc(stream)) {
            return false;
        }
    }

    return true;
}

std::string Process::readline(FILE *stream)
{
    std::string line;
    char c;

    while((c = fgetc(stream)) != EOF && !feof(stream) && c != '\n')
        line += c;

    return line;
}

void Process::print_stream(FILE *stream)
{
    char *buf = new char[80];

    while (!feof(stream)) {
        fgets(buf, 80, stream);
        std::cout << buf;
    }

    delete [] buf;
}

void Process::perform_wait()
{
    if (!finished) {
        // Wait on the child and reap it once it is complete
        int status;
        waitpid(childPid, &status, 0);

        // Check for the exit status of the child
        if (WIFEXITED(status)) {
            exitStatus = WEXITSTATUS(status);
        } else {
            abnormalExit = true;
        }

        if (WIFSIGNALED(status)) {
            signalled = true;
            signalNum = WTERMSIG(status);
        }

        // Close the files.
        if (input != NULL)
            fclose(input);

        if (output != NULL)
            fclose(output);

        if (error != NULL)
            fclose(output);
    }
}
