# Throttle policy tutorial

The **throttle** policy forwards updates no more frequently than a specified interval.

For writer/reader examples, see `docs/tutorials/README.md`.

## Example bridge.json

Use `config/tutorials/bridge-throttle.json`:

```json
{
  "version": "2.0.0",
  "time_source_type": "real",
  "transferPolicies": {
    "throttle_policy": { "type": "throttle", "intervalMs": 100 }
  },
  "nodes": [
    { "id": "node1" }
  ],
  "endpoints": [
    {
      "nodeId": "node1",
      "endpoints": [
        { "id": "n1-epSrc", "mode": "local", "config_path": "../sample/endpoint/n1-epSrc.json", "direction": "out" },
        { "id": "n1-epDst", "mode": "local", "config_path": "../sample/endpoint/n1-epDst.json", "direction": "in" }
      ]
    }
  ],
  "wireLinks": [],
  "pduKeyGroups": {
    "pdu_group1": [
      { "id": "Drone.pos", "robot_name": "Drone", "pdu_name": "pos" }
    ]
  },
  "connections": [
    {
      "id": "conn1",
      "nodeId": "node1",
      "source": { "endpointId": "n1-epSrc" },
      "destinations": [
        { "endpointId": "n1-epDst" }
      ],
      "transferPdus": [
        { "pduKeyGroupId": "pdu_group1", "policyId": "throttle_policy" }
      ]
    }
  ]
}
```

## Run

```bash
./build/hakoniwa-pdu-bridge \
  config/tutorials/bridge-throttle.json \
  1000 \
  test/config/core_flow/endpoints.json \
  node1
```

## Expected behavior

- The source can update faster than the interval.
- Transfers occur at most once per `intervalMs`.
