# AFS resource identification gate

## Current state

The Xbox 360 IOP has a read-only AFS stream path and a logical resource catalog
interface. The catalog intentionally contains no retail resource IDs yet. Archive
entry order is not a semantic identifier, and a size-only guess is not sufficient
to map a texture, model, sound bank, animation, or level.

The local inventory utility is:
- \`tools/afs_inventory.py\`
- Unit tests: \`tools/tests/test_afs_inventory.py\`

It reads \`GAMEDATA.WAG\`, \`GAMEDATA.WAH\`, \`GAMEDATA.WAI\`, and
\`GAMEDATA.WAJ\` in place, validates the AFS header/table/ranges, streams SHA-256,
and writes metadata reports. It never extracts payloads or copies game data.

## Reproduce

From PowerShell in the repository root:

\`\`\`powershell
py -3 tools\\afs_inventory.py --input-dir "C:\\Users\\Araujos\\Downloads\\is" --output-dir "afs_inventory"
\`\`\`

The output directory contains:
- \`afs_inventory.csv\`: one row per entry, including stable archive/index/range,
  SHA-256, first 64 bytes as hex, leading signature classification, sample
  statistics, embedded signature hints, overlap hints, and identical-content links.
- \`afs_inventory.json\`: the same records plus per-archive table metadata.
- \`afs_inventory_report.txt\`: archive counts and classification summary.

Only a recognized signature at byte zero can assign a primary format label.
An embedded signature, entropy value, printable ratio, or zero ratio is diagnostic
evidence only. In particular, high entropy does not prove compression or encryption.

## Evidence rules for catalog population

| Resource family | Acceptable initial evidence | Insufficient by itself |
| --- | --- | --- |
| Texture | TIM2/DDS header, validated image subresource layout, or a traced texture-load request | Large blob, high entropy, arbitrary dimensions |
| Audio | VAGp, Ogg, RIFF/WAVE, or validated game audio-bank structure | Similar sizes or adjacent entry indices |
| Video | MPEG program-stream signature plus successful stream probing | MPEG marker found only inside another blob |
| Model | Validated model/container parser, cross-reference to mesh data, or traced model-load request | Unknown binary prefix or vertex-like byte patterns |
| Level | Validated level/container parser and referenced scene/object tables, or traced level-load request | Large data size or proximity to model entries |
| Animation | Parsed animation records and skeleton/channel references, or traced animation-load request | Repeated floats or apparent keyframe-like values |

Every candidate mapping must record:
1. archive filename and entry index;
2. exact entry offset and size;
3. SHA-256 from the inventory;
4. resource family and evidence source;
5. the retail revision/disc identity used to obtain the data.

Do not infer a stable resource ID from an AFS index. Logical IDs are assigned by the
port only after the evidence is reviewed. If an entry is reused by several game
systems, record the aliases explicitly rather than duplicating or silently changing
its identity.

## Next integration gate

Once the three local metadata reports are available, review:
1. signatures and embedded signature offsets;
2. same-hash entries across archives;
3. unusual ranges/overlaps and size clusters;
4. candidate families against existing IOP/RPC traces and known load paths.

Only the reviewed mapping should populate \`kVerifiedAfsResources\`. No archive
payload, ISO, extracted asset, or copyrighted executable belongs in this repository.


## Candidate-family grouping (metadata only)

After producing `afs_inventory.json`, run:

```powershell
py -3 tools\afs_candidates.py --inventory-json afs_inventory\afs_inventory.json --output-dir afs_candidates
```

Outputs `afs_candidates.csv`, `afs_candidates.json`, and `afs_candidates_report.txt`.
Labels are candidate groups only; no heuristic output directly populates the verified resource catalog.
