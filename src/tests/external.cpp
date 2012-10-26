/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <signal.h> // For strsignal.
#include <stdio.h> // For freopen.
#include <string.h> // For strlen, strerror.

#include <sys/wait.h> // For wait (and associated macros).

#include <gtest/gtest.h>

#include <string>

#include <stout/os.hpp>
#include <stout/path.hpp>
#include <stout/strings.hpp>

#include "tests/external.hpp"
#include "tests/utils.hpp"

using std::string;

namespace mesos {
namespace internal {
namespace tests {
namespace external {

void run(const char* testCase, const char* testName)
{
  // Adjust the test name to remove any 'DISABLED_' prefix (to make
  // things easier to read). While this might seem alarming, if we are
  // "running" a disabled test it must be the case that the test was
  // explicitly enabled (e.g., via 'gtest_filter').
  if (strings::startsWith(testName, "DISABLED_")) {
    testName += strlen("DISABLED_");
  }

  // Fork a process to change directory and run the test.
  pid_t pid;
  if ((pid = fork()) == -1) {
    FAIL() << "Failed to fork to launch external test";
  }
  if (pid) {
    // In parent process.
    int status;
    while (wait(&status) != pid || WIFSTOPPED(status));
    CHECK(WIFEXITED(status) || WIFSIGNALED(status));

    if (WIFEXITED(status)) {
      if (WEXITSTATUS(status) != 0) {
        FAIL()
          << strings::join("/", testCase, testName)
          << " exited with status " << WEXITSTATUS(status);
      }
    } else {
      FAIL()
        << strings::join("/", testCase, testName)
        << " terminated with signal '" << strsignal(WTERMSIG(status)) << "'";
    }
  } else {
    // Create a temporary directory for the test.
    const string& path =
      path::join("/tmp", strings::join("_", testCase, testName, "XXXXXX"));
    Try<string> directory = os::mkdtemp(path);

    if (directory.isError()) {
      std::cerr << "Failed to create temporary directory at '"
                << path << "':" << directory.error() << std::endl;
      abort();
    }

    // Run the test out of the temporary directory we created.
    if (!os::chdir(directory.get())) {
      std::cerr << "Failed to chdir into '" << path << "'" << std::endl;
      abort();
    }

    // Redirect output to /dev/null unless the test is verbose.
    if (!flags.verbose) {
      if (freopen("/dev/null", "w", stdout) == NULL ||
          freopen("/dev/null", "w", stderr) == NULL) {
        std::cerr << "Failed to redirect stdout/stderr to /dev/null:"
                  << strerror(errno) << std::endl;
        abort();
      }
    }

    // Set up the environment for executing the script.
    os::setenv("MESOS_SOURCE_DIR", flags.source_dir);
    os::setenv("MESOS_BUILD_DIR", flags.build_dir);
    os::setenv("MESOS_WEBUI_DIR", path::join(flags.source_dir, "src", "webui"));
    os::setenv("MESOS_LAUNCHER_DIR", path::join(flags.build_dir, "src"));

    // Now execute the script.
    const string& script =
      path::join(flags.source_dir,
                 "src",
                 "tests",
                 "external",
                 testCase,
                 testName) + ".sh";

    execl(script.c_str(), script.c_str(), (char*) NULL);

    std::cerr << "Failed to execute '" << script << "': "
              << strerror(errno) << std::endl;
    abort();
  }
}

} // namespace external {
} // namespace tests {
} // namespace internal {
} // namespace mesos {
