# How to contribute

You can contribute to the documentation in several ways:
* Open an issue
* If you are partclone core developer you know what to do
* Other developers can fork the repository, modify files and make a pull request, see example below

For anything more complex than fixing a typo it is best to first discuss the proposed changes in an issue.

Example (fixing a typo):

Fork the repository on the github website (requires a github user account).

Then (replace `username` with your username):
```
git clone git@github.com:username/partclone.git
cd partclone/
```

Ensure we are on the main branch and create a new branch for our issue:
```
git branch
git checkout master
git checkout -b docs-typo-normanly
```

Change files:
```
cd docs/
fgrep -i  normanly *.scd
vi partclone.scd
vi partclone.chkimg.scd
vi partclone.dd.scd
vi partclone.disk.scd
vi partclone.restore.scd
vi partclone.info.scd
```

Create first commit, referencing the issue:
(TODO: Is issue #123 enough or should the full URL be used since the fork doesn't have any issues?)
```
git diff
git status
git add partclone.scd partclone.chkimg.scd partclone.dd.scd partclone.disk.scd partclone.restore.scd partclone.info.scd
git commit
git push --set-upstream origin docs-typo-normanly
```

The manual pages are written in the scdoc format and rendered to roff with
`scdoc(1)`. See `scdoc(5)` for the syntax. Install the `scdoc` package (for
example `scdoc` on Debian/Ubuntu, Fedora or Arch) and preview your changes
with:
```
scdoc < partclone.scd | man -l -
```

You may also derive `.8` files by hand, e.g.:
```
for I in partclone partclone.chkimg partclone.dd partclone.disk partclone.restore partclone.info ; do
    scdoc < $I.scd > $I.8
done
```

Check there are no unexpected changes than intended:
```
git diff
```

Create second commit:
```
git status
git add partclone.8 partclone.chkimg.8 partclone.dd.8 partclone.disk.8 partclone.restore.8 partclone.info.8
git commit
git push
```

Visit your fork on github. There is a button to make a pull request. If too much time passed the button may no longer be on the main page of your fork but you may find such a button when switching to the branch.
