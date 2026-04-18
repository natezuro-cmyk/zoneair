import type { AcState } from './unitClient'

export type AlertLevel = 'ok' | 'warning' | 'critical'
export type AlertSource = 'unit' | 'diagnostic'

export type Alert = {
  id: string
  level: AlertLevel
  source: AlertSource
  title: string
  detail: string
  code?: string  // e.g. "E0", "P1" for unit errors
}

const cToF = (c: number) => Math.round(c * 9 / 5 + 32)

// Known Pioneer/TCL error codes and what they mean
const ERROR_CODES: Record<number, { code: string; title: string; detail: string }> = {
  0x00: { code: 'E0', title: 'Communication failure', detail: 'Indoor and outdoor unit cannot communicate. Check wiring between units.' },
  0x01: { code: 'E1', title: 'Room temp sensor failure', detail: 'The indoor room temperature sensor (thermistor) has failed or is disconnected.' },
  0x02: { code: 'E2', title: 'Indoor coil sensor failure', detail: 'The indoor evaporator coil temperature sensor has failed or is disconnected.' },
  0x03: { code: 'E3', title: 'Outdoor coil sensor failure', detail: 'The outdoor condenser coil temperature sensor has failed or is disconnected.' },
  0x04: { code: 'E4', title: 'System abnormal', detail: 'The AC cooling system has detected an abnormality. May indicate refrigerant issue or mechanical fault.' },
  0x05: { code: 'E5', title: 'Model mismatch', detail: 'Indoor and outdoor unit models are not compatible.' },
  0x06: { code: 'E6', title: 'Indoor fan motor fault', detail: 'The indoor fan motor has failed or is not responding.' },
  0x07: { code: 'E7', title: 'Outdoor temp sensor failure', detail: 'The outdoor ambient temperature sensor has failed.' },
  0x08: { code: 'E8', title: 'Discharge temp sensor failure', detail: 'The compressor discharge temperature sensor has failed.' },
  0x09: { code: 'E9', title: 'Compressor drive fault', detail: 'IPM or compressor driving control is abnormal. May need professional service.' },
  0x0A: { code: 'EA', title: 'Current sensor fault', detail: 'The current sensing module has failed.' },
  0x0C: { code: 'EC', title: 'Outdoor communication fault', detail: 'Outdoor unit communication error. May also indicate refrigerant leak detection triggered.' },
  0x0F: { code: 'EF', title: 'Outdoor fan motor fault', detail: 'The outdoor DC fan motor has failed or is not responding.' },
  // Protection codes (F-series)
  0x12: { code: 'F2', title: 'Discharge temp protection', detail: 'Compressor discharge temperature too high. System shut down to protect compressor.' },
  0x13: { code: 'F3', title: 'Outdoor coil temp protection', detail: 'Outdoor coil temperature out of safe range.' },
  0x14: { code: 'F4', title: 'Gas flow abnormal', detail: 'Refrigerant gas flow is abnormal. Possible restriction or leak.' },
  0x18: { code: 'F8', title: 'Reversing valve fault', detail: 'The 4-way reversing valve is not switching properly between heating and cooling.' },
  0x1B: { code: 'Fb', title: 'Overload protection', detail: 'System overload protection triggered. Unit is working too hard for conditions.' },
  // Power codes (P-series)
  0x20: { code: 'P0', title: 'IPM module protection', detail: 'Hardware protection on the inverter power module triggered.' },
  0x21: { code: 'P1', title: 'Voltage protection', detail: 'Supply voltage is too high or too low for safe operation.' },
  0x22: { code: 'P2', title: 'Over-current protection', detail: 'Electrical current exceeds safe limits. May indicate compressor issue.' },
  0x24: { code: 'P4', title: 'Discharge over-temperature', detail: 'Compressor discharge temperature critically high. Often caused by low refrigerant.' },
  0x25: { code: 'P5', title: 'Sub-cooling protection', detail: 'Insufficient sub-cooling in cooling mode. May indicate low refrigerant charge.' },
  0x26: { code: 'P6', title: 'Superheat protection (cooling)', detail: 'Excessive superheat detected in cooling mode. Strong indicator of low refrigerant.' },
  0x27: { code: 'P7', title: 'Superheat protection (heating)', detail: 'Excessive superheat detected in heating mode. Strong indicator of low refrigerant.' },
  0x28: { code: 'P8', title: 'Outdoor temp protection', detail: 'Outdoor temperature is outside the operating range of the unit.' },
}

export function analyzeState(state: AcState): Alert[] {
  const alerts: Alert[] = []
  if (!state.online) return alerts

  // ── Unit-reported error codes ──
  // Bytes 20-29 in the response frame (exposed as error_code fields).
  // error_code1 is the primary error byte, error_code2 may be a sub-code.
  if (state.error_code1 !== undefined && state.error_code1 > 0) {
    const known = ERROR_CODES[state.error_code1]
    if (known) {
      alerts.push({
        id: `unit-err-${known.code}`,
        level: 'critical',
        source: 'unit',
        title: `${known.code}: ${known.title}`,
        detail: known.detail,
        code: known.code,
      })
    } else {
      alerts.push({
        id: `unit-err-unknown`,
        level: 'critical',
        source: 'unit',
        title: `Error code 0x${state.error_code1.toString(16).toUpperCase()}`,
        detail: `The unit is reporting an unrecognized error code. Contact support with code 0x${state.error_code1.toString(16).toUpperCase()}.`,
        code: `0x${state.error_code1.toString(16).toUpperCase()}`,
      })
    }
  }

  // ── Outdoor unit not communicating ──
  // When outdoor unit is disconnected, outdoor bytes are 0xFF → converted to 235°C,
  // or 0x00 → converted to -20°C. Either way, readings are wildly out of range.
  // Normal outdoor temps are -40 to 60°C. Anything outside that = no real data.
  const outdoorDisconnected =
    (state.outdoor_temp_c != null && (state.outdoor_temp_c > 80 || state.outdoor_temp_c < -30)) &&
    state.compressor_hz === 0 && state.outdoor_fan_speed === 0

  if (outdoorDisconnected) {
    alerts.push({
      id: 'outdoor-disconnect',
      level: 'critical',
      source: 'diagnostic',
      title: 'Outdoor unit not communicating',
      detail: 'No data is being received from the outdoor unit. The communication wire between indoor and outdoor units may be disconnected, damaged, or the outdoor unit may have lost power.',
    })
  }

  if (!state.power) return alerts

  const isCooling = state.mode === 1 || (state.mode === 3 && !state.four_way_valve)
  const isHeating = state.mode === 2 || (state.mode === 3 && state.four_way_valve)
  const compressorActive = state.compressor_running && state.compressor_hz > 20

  // Skip outdoor-dependent diagnostics if outdoor unit is disconnected
  const hasOutdoorData = !outdoorDisconnected

  // ── Low refrigerant (cooling) ──
  // Evaporator coil should be 10-20°C below room temp. Under 5°C delta = low charge.
  if (isCooling && compressorActive) {
    const coilDelta = state.indoor_c - state.indoor_coil_c
    if (coilDelta < 3) {
      alerts.push({
        id: 'refrig-low-cool',
        level: 'critical',
        source: 'diagnostic',
        title: 'Low refrigerant (cooling)',
        detail: `Indoor coil is ${cToF(state.indoor_coil_c)}°F but room is ${cToF(state.indoor_c)}°F — only ${Math.round(coilDelta * 9/5)}°F difference. A healthy system should have 18°F+ difference. This strongly indicates low refrigerant charge.`,
      })
    } else if (coilDelta < 7) {
      alerts.push({
        id: 'refrig-low-cool-warn',
        level: 'warning',
        source: 'diagnostic',
        title: 'Possible low refrigerant (cooling)',
        detail: `Indoor coil-to-room delta is ${Math.round(coilDelta * 9/5)}°F — lower than ideal. Monitor for worsening performance.`,
      })
    }
  }

  // ── Low refrigerant (heating) ──
  // Outdoor coil should be 5-15°C below ambient (absorbing heat). Under 3°C = low charge.
  if (isHeating && compressorActive && hasOutdoorData) {
    const outdoorDelta = state.outdoor_temp_c - state.condenser_coil_c
    if (outdoorDelta < 2) {
      alerts.push({
        id: 'refrig-low-heat',
        level: 'critical',
        source: 'diagnostic',
        title: 'Low refrigerant (heating)',
        detail: `Outdoor coil is ${cToF(state.condenser_coil_c)}°F but ambient is ${cToF(state.outdoor_temp_c)}°F — not enough difference. In heating mode the outdoor coil should be much colder than ambient to absorb heat. Indicates low refrigerant.`,
      })
    } else if (outdoorDelta < 4) {
      alerts.push({
        id: 'refrig-low-heat-warn',
        level: 'warning',
        source: 'diagnostic',
        title: 'Possible low refrigerant (heating)',
        detail: `Outdoor coil-to-ambient delta is only ${Math.round(outdoorDelta * 9/5)}°F in heating mode — lower than expected.`,
      })
    }
  }

  // ── Dirty condenser (cooling) ──
  // Condenser coil should be 10-20°C above outdoor ambient. Much higher = dirty/blocked.
  if (isCooling && compressorActive && hasOutdoorData) {
    const condenserDelta = state.condenser_coil_c - state.outdoor_temp_c
    if (condenserDelta > 25) {
      alerts.push({
        id: 'condenser-dirty',
        level: 'warning',
        source: 'diagnostic',
        title: 'Dirty outdoor coil',
        detail: `Condenser is ${cToF(state.condenser_coil_c)}°F — ${Math.round(condenserDelta * 9/5)}°F above outdoor temp. This is excessive and usually means the outdoor coil needs cleaning.`,
      })
    } else if (condenserDelta < 3) {
      alerts.push({
        id: 'condenser-low',
        level: 'warning',
        source: 'diagnostic',
        title: 'Low condenser performance',
        detail: `Condenser coil only ${Math.round(condenserDelta * 9/5)}°F above outdoor temp — may indicate low refrigerant or condenser issue.`,
      })
    }
  }

  // ── High discharge temperature ──
  if (compressorActive && hasOutdoorData && state.discharge_temp_c > 85) {
    alerts.push({
      id: 'discharge-high',
      level: state.discharge_temp_c > 100 ? 'critical' : 'warning',
      source: 'diagnostic',
      title: 'High compressor temperature',
      detail: `Compressor discharge is ${cToF(state.discharge_temp_c)}°F. ${state.discharge_temp_c > 100
        ? 'Critically high — risk of compressor damage. Often caused by low refrigerant or blocked airflow.'
        : 'Elevated — may indicate low refrigerant, dirty coils, or high ambient load.'}`,
    })
  }

  // ── Coil icing ──
  if (isCooling && compressorActive && state.indoor_coil_c < 0) {
    alerts.push({
      id: 'icing',
      level: 'critical',
      source: 'diagnostic',
      title: 'Evaporator coil icing',
      detail: `Indoor coil is ${cToF(state.indoor_coil_c)}°F — below freezing. Ice is forming on the evaporator. Check air filter and airflow. Can also be caused by low refrigerant.`,
    })
  }

  // ── Filter ──
  if (state.filter_alert) {
    alerts.push({
      id: 'filter',
      level: 'warning',
      source: 'diagnostic',
      title: 'Clean air filter',
      detail: 'The unit reports the air filter needs cleaning. A dirty filter reduces efficiency, increases energy use, and can cause the evaporator coil to freeze.',
    })
  }

  // ── Voltage issues ──
  if (state.supply_voltage_raw > 0 && state.supply_voltage_raw < 190) {
    alerts.push({
      id: 'voltage-low',
      level: 'warning',
      source: 'diagnostic',
      title: 'Low supply voltage',
      detail: `Voltage reading is low (${state.supply_voltage_raw}). Low voltage can prevent the compressor from starting and cause premature failure.`,
    })
  }
  if (state.supply_voltage_raw > 260) {
    alerts.push({
      id: 'voltage-high',
      level: 'warning',
      source: 'diagnostic',
      title: 'High supply voltage',
      detail: `Voltage reading is high (${state.supply_voltage_raw}). High voltage can damage electrical components.`,
    })
  }

  // ── Compressor not starting ──
  if ((isCooling || isHeating) && !state.compressor_running && state.compressor_hz === 0) {
    const roomDelta = Math.abs(state.indoor_c - state.setpoint_c)
    if (roomDelta > 3) {
      alerts.push({
        id: 'compressor-idle',
        level: 'warning',
        source: 'diagnostic',
        title: 'Compressor not running',
        detail: `Room is ${cToF(state.indoor_c)}°F but set to ${cToF(state.setpoint_c)}°F. Compressor may be in a startup delay (normal for 3 min after power-on) or there may be an issue preventing it from starting.`,
      })
    }
  }

  // ── Antifreeze mode active ──
  if (state.antifreeze) {
    alerts.push({
      id: 'antifreeze',
      level: 'warning',
      source: 'unit',
      title: 'Freeze protection active',
      detail: 'The unit has activated its 8°C freeze protection mode to prevent pipes from freezing. This overrides normal operation.',
    })
  }

  return alerts
}

// Summarize the overall status for the header badge
export function getOverallStatus(alerts: Alert[]): { level: AlertLevel; label: string } {
  const hasCritical = alerts.some(a => a.level === 'critical')
  const hasWarning = alerts.some(a => a.level === 'warning')
  if (hasCritical) return { level: 'critical', label: 'Error' }
  if (hasWarning) return { level: 'warning', label: 'Maintenance' }
  return { level: 'ok', label: 'OK' }
}
