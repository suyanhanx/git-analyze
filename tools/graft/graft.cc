/// THIS IS GRAFT COMMAND
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <argvex.hpp>
#include <git.hpp>
#include <optional>

struct graft_info_t {
  std::string gitdir{"."};
  std::string branch;
  std::string commitid;
  std::string message;
};

void PrintUsage() {
  const char *ua = R"(OVERVIEW: git-graft
Usage: git-graft [options] <input>
OPTIONS:
  -h [--help]                      print git-graft usage information and exit
  -b [--branch]                    new branch name.
  -d [--git-dir]                   repository path.
  -m [--message]                   commit message
Example:
  git-graft commit-id -m "message"
)";
  printf("%s", ua);
}

bool ParseArgv(int argc, char **argv, graft_info_t &gf) {
  std::vector<ax::ParseArgv::option> opts = {
      {"help", ax::ParseArgv::no_argument, 'h'},
      {"git-dir", ax::ParseArgv::required_argument, 'd'},
      {"message", ax::ParseArgv::required_argument, 'm'},
      {"branch", ax::ParseArgv::required_argument, 'b'}
      //
  };
  ax::ParseArgv pv(argc, argv);
  auto err =
      pv.ParseArgument(opts, [&](int ch, const char *optarg, const char *raw) {
        switch (ch) {
        case 'h':
          PrintUsage();
          exit(0);
          break;
        case 'b':
          gf.branch = optarg;
          break;
        case 'm':
          gf.message = optarg;
          break;
        case 'd':
          gf.gitdir = optarg;
          break;
        default:
          printf("Error Argument: %s\n", raw != nullptr ? raw : "unknown");
          return false;
        }
        return true;
      });
  if (err.errorcode != 0) {
    if (err.errorcode == 1) {
      printf("ParseArgv: %s\n", err.message.c_str());
    }
    return false;
  }
  if (pv.UnresolvedArgs().size() < 1) {
    PrintUsage();
    return false;
  }
  gf.commitid = pv.UnresolvedArgs()[0];
  return true;
}

inline void SignatureCommiterFill(git_signature *sig,
                                  const git_signature *old) {
  sig->when = old->when;
  auto name = getenv("GIT_COMMITTER_NAME");
  if (name != nullptr) {
    sig->name = name;
  } else {
    sig->name = old->name;
  }
  auto email = getenv("GIT_COMMITTER_EMAIL");
  if (email != nullptr) {
    sig->email = email;
  } else {
    sig->email = old->email;
  }
}
void SignatureAuthorFill(git_signature *sig, const git_signature *old) {
  sig->when = old->when;
  auto name = getenv("GIT_AUTHOR_NAME");
  if (name != nullptr) {
    sig->name = name;
  } else {
    sig->name = old->name;
  }
  auto email = getenv("GIT_AUTHOR_NAME");
  if (email != nullptr) {
    sig->email = email;
  } else {
    sig->email = old->email;
  }
}

std::optional<std::string> Refernece(git_repository *repo,
                                     const std::string &b) {
  if (!b.empty()) {
    return std::make_optional("refs/heads/" + b);
  }
  git_reference *ref = nullptr;
  if (git_reference_lookup(&ref, repo, "HEAD") != 0) {
    return std::nullopt;
  }
  git_reference *df = nullptr;
  if (git_reference_resolve(&df, ref) != 0) {
    git_reference_free(ref);
    return std::nullopt;
  }
  std::string name = git_reference_name(df);
  git_reference_free(ref);
  git_reference_free(df);
  return std::make_optional(name);
}

bool GraftCommit(const graft_info_t &gf) {
  git::Initializer initializer;
  git::Repository repo;
  if (!repo.open(gf.gitdir)) {
    git::PrintError();
    return false;
  }
  git::Commit commit;
  if (!commit.open(repo.pointer(), gf.commitid)) {
    fprintf(stderr, "open commit: %s ", gf.commitid.c_str());
    git::PrintError();
    return false;
  }
  auto ref = Refernece(repo.pointer(), gf.branch);
  if (!ref) {
    fprintf(stderr, "cannot found ref\n");
    return false;
  }
  git::Commit parent;
  if (!parent.open_ref(repo.pointer(), *ref)) {
    fprintf(stderr, "open par commit: %s ", ref->c_str());
    git::PrintError();
    return false;
  }
  ///

  git_tree *tree = nullptr;
  if (git_commit_tree(&tree, commit.pointer()) != 0) {
    git::PrintError();
    return false;
  }
  git_signature author, committer;
  SignatureAuthorFill(&author, git_commit_author(commit.pointer()));
  SignatureCommiterFill(&committer, git_commit_committer(commit.pointer()));
  std::string msg =
      gf.message.empty() ? git_commit_message(commit.pointer()) : gf.message;
  const git_commit *parents[] = {parent.pointer(), commit.pointer()};
  fprintf(stderr, "New commit, message: '%s'\n", msg.c_str());
  git_oid oid;
  if (git_commit_create(&oid, repo.pointer(), ref->c_str(), &author, &committer,
                        nullptr, msg.c_str(), tree, 2, parents) != 0) {
    git::PrintError();
    git_tree_free(tree);
    return false;
  }
  fprintf(stderr, "New commit id: %s\n", git_oid_tostr_s(&oid));
  git_tree_free(tree);
  return true;
}

int cmd_main(int argc, char **argv) {
  graft_info_t gf;
  if (!ParseArgv(argc, argv, gf)) {
    return 1;
  }
  if (!GraftCommit(gf)) {
    return 1;
  }
  return 0;
}
