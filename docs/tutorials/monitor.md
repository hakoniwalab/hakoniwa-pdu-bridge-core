# On-demand monitor tutorial

This tutorial runs the existing immediate bridge flow and enables on-demand monitoring via `EndpointMux`.

## Config files

- Mux endpoint (bridge side): `config/tutorials/monitor/mux_endpoint.json`
- Mux comm (bridge side): `config/tutorials/monitor/mux_comm_tcp_server_9200.json`
- Monitor client endpoint: `config/tutorials/monitor/client_endpoint.json`
- Monitor client comm: `config/tutorials/monitor/client_comm_tcp_9200.json`

## Run

Terminal 1 (bridge daemon + on-demand):
```bash
./cmake-build/hakoniwa-pdu-bridge \
  config/tutorials/bridge-immediate.json \
  1000 \
  config/tutorials/endpoint_container.json \
  node1 \
  --enable-ondemand \
  --ondemand-mux-config config/tutorials/monitor/mux_endpoint.json
```

Terminal 2 (reader, optional but useful for comparison):
```bash
cmake-build/examples/bridge_reader \
  config/tutorials/endpoint/reader.json \
  Drone \
  pos \
  10
```

Terminal 3 (writer):
```bash
cmake-build/examples/bridge_writer \
  config/tutorials/endpoint/writer.json \
  Drone \
  pos \
  20
```

Terminal 4 (monitor control/data plane):
```bash
# health
cmake-build/hakoniwa-pdu-bridge-monitor config/tutorials/monitor/client_endpoint.json health

# connection list
cmake-build/hakoniwa-pdu-bridge-monitor config/tutorials/monitor/client_endpoint.json connections

# PDU list in conn1
cmake-build/hakoniwa-pdu-bridge-monitor config/tutorials/monitor/client_endpoint.json list_pdus conn1

# tail for 10 seconds
cmake-build/hakoniwa-pdu-bridge-monitor config/tutorials/monitor/client_endpoint.json tail conn1 throttle 100 10

# tail until Ctrl-C
cmake-build/hakoniwa-pdu-bridge-monitor config/tutorials/monitor/client_endpoint.json tail conn1 throttle 100
```

Expected `tail` output block:
```text
[monitor-data]
  timestamp_usec: ...
  robot: Drone
  channel_id: 1
  pdu_name: pos
  payload_size: 72
  epoch: N/A
```

## Notes

- `conn1` comes from `config/tutorials/bridge-immediate.json`.
- `tail` internally does `list_pdus` + `subscribe`, and auto `unsubscribe` on exit.
- `tail <connection_id> throttle 100 10` means: throttle policy, 100ms interval, auto-stop after 10 seconds.
- `epoch` is `N/A` when payload is not in Hakoniwa PDU metadata format.
- To connect multiple monitor clients, increase `expected_clients` in `mux_comm_tcp_server_9200.json`.
- `[monitor][event] control session connected/disconnected` is expected when the monitor CLI process starts/exits.
