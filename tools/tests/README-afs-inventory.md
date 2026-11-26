# AFS inventory tool

The tool reads the four retail AFS containers in-place. It does not extract, modify,
or publish game data. For every entry it records archive/index/range, SHA-256,
a 64-byte prefix in hex, conservative leading-signature classification, sample
entropy/printable/zero ratios, embedded signature hints, partial range overlaps,
and identical payload locations.

From PowerShell, with the repository cloned:

\`\`\`powershell
py -3 tools\\afs_inventory.py --input-dir "C:\\Users\\Araujos\\Downloads\\is" --output-dir "afs_inventory"
\`\`\`

Expected outputs:
- \`afs_inventory\\afs_inventory.csv\`
- \`afs_inventory\\afs_inventory.json\`
- \`afs_inventory\\afs_inventory_report.txt\`

Send only these metadata reports back for analysis. Do not send archive payloads,
ISO images, or extracted assets. Unknown entries intentionally stay unclassified.
Embedded signatures and entropy are hints, not proof of a resource type.
