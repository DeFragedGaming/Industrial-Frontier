Stage 10H-2 - Permanent Linux CI

Copy these complete replacements into the Industrial Frontier repo:
.github/workflows/linux-build.yml
tools/linux/package.sh
docs/LINUX_BUILD.md

Do not rename the workflow each stage. Push new gameplay changes normally; Linux builds package under the same name.

Ensure all C++ test source files exist and are tracked before pushing.
