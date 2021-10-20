/**
 * File: trace.cc
 * ----------------
 * Presents the implementation of the trace program, which traces the execution of another
 * program and prints out information about ever single system call it makes.  For each system call,
 * trace prints:
 *
 *    + the name of the system call,
 *    + the values of all of its arguments, and
 *    + the system calls return value
 */

#include <cassert>
#include <iostream>
#include <map>
#include <set>
#include <unistd.h> // for fork, execvp
#include <string.h> // for memchr, strerror
#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/wait.h>
#include <sstream>
#include "trace-options.h"
#include "trace-error-constants.h"
#include "trace-system-calls.h"
#include "trace-exception.h"
using namespace std;

std::pair<long, bool> trace_child_syscall(pid_t pid, int reg)
{
  int status;
  while (true) {
    ptrace(PTRACE_SYSCALL, pid, 0, 0);
    waitpid(pid, &status, 0);
    if (WIFSTOPPED(status) && (WSTOPSIG(status) == (SIGTRAP | 0x80))) {
      long num = ptrace(PTRACE_PEEKUSER, pid, reg * sizeof(long));
      return {num, false};
    }else if(WIFEXITED(status))
    {
      return {WEXITSTATUS(status), true};
    }
  }
}

string readString(pid_t pid, long string_address)
{
  string result = "";
  bool finished{false};
  while(!finished)
  {
    long val = ptrace(PTRACE_PEEKDATA, pid, string_address);
    char *p_val = (char *)(&val);
    for(int i = 0; i < static_cast<int>(sizeof(long)); i++)
    {
      if(p_val[i] == '\0')
      {
        finished = true;
        break;
      }
      result.push_back(p_val[i]);
    }
    string_address += 8;
  }
  return result;
}

string get_hex_string(long value)
{
  std::stringstream sstream;
  sstream << std::hex << value;
  return sstream.str();
}

string get_param_string(pid_t pid, long param_value, scParamType param_type)
{
  // interpret retrieved param value
  switch (param_type)
  {
    case SYSCALL_INTEGER:
      return to_string(static_cast<int>(param_value));
    break;
    case SYSCALL_STRING: 
      return readString(pid, param_value);
    break;
    case SYSCALL_POINTER: 
      if(param_value == 0)
      {
        return "NULL";
      }else
      {
        return get_hex_string(param_value);
      }
    break;
    case SYSCALL_UNKNOWN_TYPE:
    default:
      return "<unknown>";
    break;
  }
}

string get_function_call_string(pid_t pid, int operation_code, bool simple,   
                                const std::map<int, std::string> &systemCallNumbers,
                                const std::map<std::string, int> &systemCallNames,
                                const std::map<std::string, systemCallSignature> &systemCallSignatures)
{
  if(simple)
  {
    return "syscall(" + to_string(operation_code) + ") = ";
  }

  std::string result{""};
  std::string func_name = systemCallNumbers.at(operation_code);
  result += func_name;

  result.push_back('(');
  if(systemCallSignatures.find(func_name) == systemCallSignatures.end())
  {
    result += "<signature-information-missing>";
  }else
  {
    static const int params_registers[] = {RDI, RSI, RDX, R10, R8, R9};
    const systemCallSignature& signature_info = systemCallSignatures.at(func_name);
    for(size_t i = 0; i < signature_info.size(); i++)
    {
      long param_value = ptrace(PTRACE_PEEKUSER, pid, params_registers[i] * sizeof(long));
      string param_string = get_param_string(pid, param_value, signature_info[i]);
      result += param_string;
      if(i < signature_info.size() - 1) 
      {
        result += ", ";
      }
    }
  }
  result.push_back(')');

  return result;
}

string get_function_return_value_string(pid_t pid, int op_code, long return_value, bool simple, 
                                        const std::map<int, std::string> &systemCallNumbers,
                                        const std::map<int, std::string> &errorConstants)
{
  if(simple) 
  {
    return to_string(return_value);
  }
  else
  {
    string fun_name = systemCallNumbers.at(op_code);
    if(fun_name == "brk" || fun_name == "mmap")
    {
      return get_hex_string(return_value);
    }else if(return_value >= 0)
    {
      return to_string(static_cast<int>(return_value));
    }else
    {
      string result = "-1 ";
      int error_number = static_cast<int>(-return_value);
      result += errorConstants.at(error_number);
      result += " (" + string(strerror(error_number)) + ")";
      return result;
    }
  }
  return "";
}

int main(int argc, char *argv[]) {
  bool simple = false, rebuild = false;
  int numFlags = processCommandLineFlags(simple, rebuild, argv);
  if (argc - numFlags == 1) {
    cout << "Nothing to trace... exiting." << endl;
    return 0;
  }

  pid_t pid = fork();
  if (pid == 0) {
    ptrace(PTRACE_TRACEME);
    raise(SIGSTOP);
    execvp(argv[numFlags + 1], argv + numFlags + 1);
    return 0;
  }

  // parent prepares the tracing 
  int status;
  waitpid(pid, &status, 0);
  assert(WIFSTOPPED(status));
  ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_TRACESYSGOOD);


  std::map<int, std::string> systemCallNumbers;
  std::map<std::string, int> systemCallNames;
  std::map<std::string, systemCallSignature> systemCallSignatures;
  std::map<int, std::string> errorConstants;
  
  if(!simple)
  {
    compileSystemCallData(systemCallNumbers, systemCallNames, systemCallSignatures, rebuild);
    compileSystemCallErrorStrings(errorConstants);
    // for(auto it = systemCallSignatures.begin(); it != systemCallSignatures.end(); ++it)
    // {
    //   std::cout << it->first << " " << it->second.size() << "\n";
    // }
  }

  int ret_status = 0;
  while(true)
  {
    // trace child just before syscall starts
    auto p1 = trace_child_syscall(pid, ORIG_RAX);
    if(p1.second) 
    {
      ret_status = static_cast<int>(p1.first);
      break;
    }
    string function_call_string = get_function_call_string(pid, static_cast<int>(p1.first), simple, systemCallNumbers, systemCallNames, systemCallSignatures);
    cout << function_call_string << " = " << flush;

    // trace tracee when just exited the system call
    auto p2 = trace_child_syscall(pid, RAX);
    if(p2.second) 
    {
      cout << "<no return>" << endl;
      ret_status = static_cast<int>(p2.first);
      break;
    }
    cout << get_function_return_value_string(pid, static_cast<int>(p1.first), p2.first, simple, systemCallNumbers, errorConstants) << endl;
  }
  std::cout << "Program exited normally with status " << ret_status << '\n' << flush;

  return ret_status;
}
