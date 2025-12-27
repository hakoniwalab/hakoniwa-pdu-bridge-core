# AGENTS.md

You must implement this repository according to the following documents,
in the given order of priority:

1. impl-design.md
2. README.md
3. Any README.md under subdirectories, if relevant

Rules:
- Follow impl-design.md strictly. Do not reinterpret or redesign.
- One class per file.
- Do NOT implement endpoint loaders.
  Endpoint loading is provided by hakoniwa-pdu-endpoint.
- This repository implements bridge-side logic only.
- Do not invent new abstractions or configuration formats.
- Language: C++20
- Exception-free design.

If there is a conflict:
impl-design.md > README.md > subdirectory README.md
