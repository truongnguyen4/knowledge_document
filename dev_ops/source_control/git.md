# Git
**Git** is a distributed version control system. Every clone contains the full history, so most operations run locally without a network connection.

## Git Areas
A file travels through four areas before it reaches the team.

```text
  working directory        staging area          local repository        remote repository
   (your files)            (index)               (.git commits)          (origin)

        |  git add  ------------>  |                    |                        |
        |                          |  git commit ---->  |                        |
        |                          |                    |  git push ---------->  |
        |  <----------------------------- git checkout / git switch              |
        |  <-------------- git reset                    |  <---- git fetch ------|
```

## Ignoring Files
Files listed in `.gitignore` stay untracked and never reach the staging area.
```text
# Ignore folders
build/
# Ignore files
*.o
# Ignore environment files
.env
```
> `.gitignore` only affects untracked files. A file that is already tracked keeps being tracked until you run `git rm --cached`.
