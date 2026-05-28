# YAcFS Metrics — Prometheus Exporter

Exposes YAcFS pool statistics in Prometheus text format for monitoring and alerting.

## Usage

```bash
yacfs-metrics /data/pool 9101
```

Then configure Prometheus to scrape `http://host:9101/metrics`.

## Metrics

| Metric | Type | Description |
|---|---|---|
| `yacfs_inodes_total` | gauge | Number of inodes in pool |
| `yacfs_blocks_total` | gauge | Number of block files |
| `yacfs_snapshots_total` | gauge | Number of snapshots |
| `yacfs_block_bytes` | gauge | Total block storage size |
| `yacfs_meta_bytes` | gauge | Total metadata size |

## Example Prometheus Config

```yaml
scrape_configs:
  - job_name: 'yacfs'
    static_configs:
      - targets: ['localhost:9101']
```

## Grafana Dashboard

Import the YAcFS dashboard from `grafana/dashboard.json` (included in this repo).
