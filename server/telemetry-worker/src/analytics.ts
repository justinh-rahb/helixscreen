// SPDX-License-Identifier: GPL-3.0-or-later
// Maps telemetry events to Analytics Engine data points for real-time querying.

export interface AnalyticsEngineDataPoint {
  blobs?: string[];
  doubles?: number[];
  indexes?: string[];
}

/**
 * Maps a raw telemetry event to an Analytics Engine data point.
 * Each event type has a specific schema for blobs/doubles to enable
 * efficient SQL queries via the Analytics Engine SQL API.
 */
export function mapEventToDataPoint(
  event: Record<string, unknown>,
): AnalyticsEngineDataPoint {
  const eventType = String(event.event ?? "unknown");
  const deviceId = String(event.device_id ?? "");

  if (eventType === "session") {
    return {
      indexes: ["session"],
      blobs: [
        deviceId,
        String(event.version ?? ""),
        String(event.platform ?? ""),
        String(event.printer_model ?? ""),
        String(event.kinematics ?? ""),
        String(event.display ?? ""),
        String(event.locale ?? ""),
        String(event.theme ?? ""),
        String(event.arch ?? ""),
        "",
        "",
        "",
      ],
      doubles: [
        Number(event.ram_total_mb ?? 0),
        Number(event.cpu_cores ?? 0),
        Number(event.extruder_count ?? 0),
        0,
        0,
        0,
        0,
        0,
      ],
    };
  }

  if (eventType === "print_outcome") {
    return {
      indexes: ["print_outcome"],
      blobs: [
        deviceId,
        String(event.outcome ?? ""),
        String(event.filament_type ?? ""),
        String(event.version ?? ""),
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
      ],
      doubles: [
        Number(event.duration_sec ?? 0),
        Number(event.nozzle_temp ?? 0),
        Number(event.bed_temp ?? 0),
        Number(event.filament_used_mm ?? 0),
        Number(event.phases_completed ?? 0),
        0,
        0,
        0,
      ],
    };
  }

  if (eventType === "crash") {
    return {
      indexes: ["crash"],
      blobs: [
        deviceId,
        String(event.version ?? ""),
        String(event.signal_name ?? ""),
        String(event.platform ?? ""),
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
      ],
      doubles: [
        Number(event.uptime_sec ?? 0),
        Number(event.signal_number ?? 0),
        Number(event.backtrace_depth ?? 0),
        0,
        0,
        0,
        0,
        0,
      ],
    };
  }

  // Unknown event type — still write basic info for discoverability
  return {
    indexes: [eventType],
    blobs: [deviceId, eventType, "", "", "", "", "", "", "", "", "", ""],
    doubles: [0, 0, 0, 0, 0, 0, 0, 0],
  };
}
