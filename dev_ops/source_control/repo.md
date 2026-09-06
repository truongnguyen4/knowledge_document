# Repo
**Repo** is a Python tool that manages many Git repositories at once. It does not replace Git: it clones a list of Git projects described in an XML **manifest**, keeps them at the right revision, and runs Git commands across all of them.

> Repo is used when a product is built from dozens or hundreds of separate Git projects, such as an Android/AOSP tree or a firmware platform.

## Source Structure
A repo workspace, called a **client**, looks like this:

```text
workspace/
├── .repo/                     # everything repo owns
│   ├── repo/                  # the repo tool itself, a git clone
│   ├── manifests/             # git clone of the manifest repository
│   │   ├── default.xml        # main manifest file
│   │   └── include/
│   │       ├── platform.xml   # sub-manifest for platform-specific projects
│   │       └── apps.xml       # sub-manifest for application-specific projects
│   ├── manifests.git/         # bare copy of the manifest repository
│   ├── manifest.xml           # points to the selected manifest
│   └── projects/              # git metadata for every project
...
```


## Understanding Manifests structure
### 1. Structure part of the manifest repository
```text
manifests/
├── default.xml             # main manifest 
├── include/
|    ├── platform.xml       # sub-manifest
|    ├── drivers.xml        # sub-manifest
|    └── ...
└── manifest.xml            # selected manifest (after repo init -m <default.xml>)
```

### 2. Main Manifest including Sub-Manifests
`default.xml` includes the sub-manifests for specific domains.
```xml
<?xml version="1.0" encoding="UTF-8"?>
<manifest>
  <remote name="origin" fetch="ssh://git@server/platform" review="https://gerrit.company.com" />
  <default remote="origin" revision="release-1.0" sync-j="8" />
  <include name="include/platform.xml" />
  <include name="include/drivers.xml" />
  <include name="include/apps.xml" />
  <include name="include/tools.xml" />
</manifest>
```

### 3. Sub-Manifest declare the projects it contains
A sub-manifest is a normal manifest with the same root tag. It does not repeat `<remote>` or `<default>`, because the including file already defined them.
```xml
<?xml version="1.0" encoding="UTF-8"?>
<manifest>
  <project name="drivers/camera" path="drivers/camera" groups="drivers" />
  <project name="drivers/audio"  path="drivers/audio"  groups="drivers" />
  <project name="kernel/common"  path="kernel"         groups="drivers" revision="refs/tags/k5.15" />
</manifest>
```


## Usage

### 1. Initialize a repo
```bash
repo init -u ssh://git@server/platform/manifest.git -b release-1.0 -m default.xml
```
| Option | Description |
| --- | --- |
| `-u` | URL of the **manifest repository**. |
| `-b` | Branch of the manifest repository, such as `main` or `release-1.0`. |
| `-m` | Manifest file to use inside that repository. The default is `default.xml`. |
| `-g` | Specify which groups of projects to sync. For example, `-g default,vendor` will sync only the `default` and `vendor` groups. |
| `--depth=1` | Shallow clone, to save disk and time when history is not needed. |
| `--reference=/path/to/mirror` | Reuse an existing local mirror instead of downloading everything again. |

### 2. Sync the Projects
```bash
repo sync
```

## Manifest File

### 1. Manifest Overview
The manifest is an XML file that lists **which** repositories to clone, **where** to put them, and **which** revision to check out.
```xml
<?xml version="1.0" encoding="UTF-8"?>
<manifest>
  <remote name="origin"
          fetch="ssh://git@server/platform"
          review="https://gerrit.company.com"
          revision="refs/heads/main" />

  <default remote="origin"
           revision="release-1.0"
           sync-j="8" />

  <project name="build/make" path="build" groups="pdk" />

  <project name="frameworks/base"
           path="frameworks/base"
           revision="refs/tags/v1.4.2"
           groups="platform,notdefault">
    <linkfile src="Android.bp" dest="out/Android.bp" />
  </project>

  <project name="vendor/company/app" path="vendor/company/app" clone-depth="1" />
</manifest>
```

### Tags
| Tag | Description |
| --- | --- |
| `<manifest>` | Root element. Contains every other tag. |
| `<remote>` | A Git server. `name` is referenced by projects, `fetch` is the base URL, `review` is the Gerrit server used by `repo upload`. |
| `<default>` | Values applied to every project that does not set them, such as `remote` and `revision`. |
| `<project>` | One Git repository to clone into the client. |
| `<include>` | Pulls in another manifest file. Used to split a large manifest into parts. |
| `<linkfile>` | Creates a **symlink** outside the project, pointing at a file inside it. Nothing is duplicated, so the link always follows the project content. |

### Attributes
| Attribute | Applies to | Description |
| --- | --- | --- |
| `name` | `project` | Path of the repository **on the server**, appended to the remote `fetch` URL. |
| `path` | `project` | Directory **in the client** where the project is checked out. |
| `remote` | `project` | Which `<remote>` to fetch from, when it is not the default one. |
| `revision` | `project`, `default`, `remote` | Branch, tag, or commit SHA-1 to check out. |
| `groups` | `project` | Comma-separated labels used to sync only a part of the tree. |

### Default Revision
Repo looks for a revision in this order and uses the first one it finds:
| Order | Source |
| --- | --- |
| 1 | `revision` on the `<project>`. |
| 2 | `revision` on `<default>`. |
| 3 | `revision` on the `<remote>` used by the project. |

If none of them is set, `repo sync` fails with a missing revision error.

> `repo init -b <branch>` selects the branch of the **manifest repository**, not the branch of the projects. The project branch comes from the `revision` attributes above.
>
> A `revision` may be a branch (`main`), a tag (`refs/tags/v1.4.2`), or a commit SHA-1. A SHA-1 pins the project, so the build is reproducible.

## Useful Commands
### repo sync
Fetches every project and updates the working tree.
```bash
repo sync -c -j8 --no-tags --optimized-fetch
```
| Option | Description |
| --- | --- |
| `-d`, `--detach` | Detach the projects back to the manifest revision, leaving local branches in place but not checked out. |
| `-f`, `--force-broken` | Continue with the other projects when one fails. |
| `--force-sync` | Overwrite a project whose path changed in the manifest. |
| `--no-tags` | Skip tags, reducing the download size. |
| `--prune` | Delete remote-tracking branches that no longer exist on the server. |
| `--fail-fast` | Stop at the first failing project. |

*First sync of a large tree*
```bash
repo sync -c -j$(nproc) --no-tags --fail-fast
```

*Daily update*
```bash
repo sync -c -j8 --optimized-fetch
```

### repo forall
Runs the same shell command in every project.
```bash
repo forall -c 'git status -s'
```
| Option | Description |
| --- | --- |
| `-c` | The command to run. Everything after it is passed to the shell. |
| `-p` | Print the project header before the output. |
| `-v` | Show stderr as well. |
| `-g <groups>` | Limit to the projects in these groups. |
| `-e`, `--abort-on-errors` | Stop as soon as a command returns non-zero. |

Repo exports these variables inside the command:
| Variable | Description |
| --- | --- |
| `$REPO_PROJECT` | Project `name` from the manifest. |
| `$REPO_PATH` | Project path in the client. |
| `$REPO_REMOTE` | Remote name. |
| `$REPO_RREV` | Revision as written in the manifest. |
| `$REPO_LREV` | Same revision resolved to a SHA-1. |

### Other Commands
| Command | Description |
| --- | --- |
| `repo manifest -r -o <manifest>` | Write the current manifest with every project pinned to its SHA-1. |
| `repo diffmanifest <manifest> <other-manifest>` | Show the differences between the current manifest and the one in the repository. |
| `repo abandon <branch>` | Delete a topic branch from every project. |
| `repo branches` | List the topic branches and the projects they exist in. |
| `repo info` | Show the manifest branch, the remote, and the revision of each project. |
> `repo manifest -r -o snapshot.xml` is the standard way to archive a release. Committing that snapshot to the manifest repository lets anyone rebuild the exact same tree later with `repo init -m snapshot.xml`.
