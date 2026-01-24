# Immediate policy tutorial

The **immediate** policy forwards a PDU as soon as it is updated on the source endpoint.

For writer/reader examples, see `docs/tutorials/README.md`.

## Example bridge.json

Use `config/tutorials/bridge-immediate.json`:

```json
{
  "version": "2.0.0",
  "time_source_type": "real",
  "transferPolicies": {
    "immediate_policy": { "type": "immediate" }
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
        { "pduKeyGroupId": "pdu_group1", "policyId": "immediate_policy" }
      ]
    }
  ]
}
```

## Run

```bash
./build/hakoniwa-pdu-bridge \
  config/tutorials/bridge-immediate.json \
  1000 \
  test/config/core_flow/endpoints.json \
  node1
```

## Expected behavior

- Every write on the source endpoint triggers a transfer to the destination.
- The time source is not used for this policy.

## Atomic immediate (optional)

You can make an immediate policy atomic by setting `atomic: true`:

```json
"immediate_policy": { "type": "immediate", "atomic": true }
```

When atomic, the bridge waits until all PDUs in the transfer group have updated before sending them together.
