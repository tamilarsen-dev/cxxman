#include "sys/process.hpp"

#ifndef _WIN32
#include <cerrno>
#include <cstddef>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <vector>

extern char** environ;
#endif

namespace cxxman::sys {

#ifdef _WIN32

int run(std::span<const std::string>, std::string_view) { return -1; }

#else

int run(std::span<const std::string> argv, std::string_view input) {
  if (argv.empty()) return -1;
  int pipe_fd[2];
  if (pipe(pipe_fd) != 0) return -1;
  // The write end must not leak into the child, or it never sees EOF.
  fcntl(pipe_fd[1], F_SETFD, FD_CLOEXEC);

  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_adddup2(&actions, pipe_fd[0], STDIN_FILENO);
  posix_spawn_file_actions_addclose(&actions, pipe_fd[0]);
  posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
  posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);

  std::vector<char*> args;
  for (const auto& a : argv) args.push_back(const_cast<char*>(a.c_str()));
  args.push_back(nullptr);

  pid_t pid = 0;
  const int spawned = posix_spawnp(&pid, args[0], &actions, nullptr, args.data(), environ);
  posix_spawn_file_actions_destroy(&actions);
  close(pipe_fd[0]);
  if (spawned != 0) {
    close(pipe_fd[1]);
    return -1;
  }

  // SIGPIPE is ignored (term::Console), so a child that stops reading early
  // only makes write() fail.
  for (std::size_t done = 0; done < input.size();) {
    const ssize_t n = write(pipe_fd[1], input.data() + done, input.size() - done);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) break;
    done += static_cast<std::size_t>(n);
  }
  close(pipe_fd[1]);

  int status = 0;
  while (waitpid(pid, &status, 0) < 0)
    if (errno != EINTR) return -1;
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

#endif

}  // namespace cxxman::sys
