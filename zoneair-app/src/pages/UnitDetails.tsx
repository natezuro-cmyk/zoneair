import { useEffect, useState } from 'react'
import type { Unit } from '../state/units'
import { UnitClient } from '../lib/unitClient'
import type { AcState } from '../lib/unitClient'
import { analyzeState, getOverallStatus } from '../lib/alerts'
import type { Alert } from '../lib/alerts'

type Props = { unit: Unit; onBack: () => void; onSwitchUnit?: () => void }

const cToF = (c: number) => Math.round(c * 9 / 5 + 32)
const safeF = (c: number | undefined) => c != null && isFinite(c) && c > -40 && c < 120 ? `${cToF(c)}°F` : '—'
const safeNum = (n: number | undefined) => n != null && isFinite(n) ? String(n) : '—'

const MODE_LABELS = ['Off', 'Cool', 'Heat', 'Auto', 'Dry', 'Fan']
const FAN_LABELS = ['Auto', '1', '2', '3', '4', '5']

export default function UnitDetails({ unit, onBack, onSwitchUnit }: Props) {
  const [client] = useState(() => new UnitClient(unit.host))
  const [state, setState] = useState<AcState | null>(null)
  const [alerts, setAlerts] = useState<Alert[]>([])

  useEffect(() => {
    let alive = true
    let ws: WebSocket | null = null
    client.getState().then(s => { if (alive) { setState(s); setAlerts(analyzeState(s)) } }).catch(() => {})
    try {
      ws = client.openSocket(s => {
        if (alive) { setState(s); setAlerts(analyzeState(s)) }
      })
    } catch {}
    return () => { alive = false; ws?.close() }
  }, [client])

  if (!state) return (
    <div className="flex flex-col items-center justify-center h-full">
      <div className="spinner" />
      <div className="text-mute text-sm mt-4">Connecting...</div>
      <button onClick={onBack} className="mt-6 text-sm text-dim">Back</button>
    </div>
  )

  const status = getOverallStatus(alerts)
  const unitErrors = alerts.filter(a => a.source === 'unit')
  const diagnosticAlerts = alerts.filter(a => a.source === 'diagnostic')
  const modeLabel = MODE_LABELS[state.mode] ?? '—'
  const fanLabel = FAN_LABELS[state.fan] ?? '—'

  // Detect if outdoor data is available
  // Disconnected outdoor unit sends 0xFF bytes → 235°C after conversion, or 0x00 → -20°C
  const hasOutdoorData = state.outdoor_temp_c != null &&
    state.outdoor_temp_c > -30 && state.outdoor_temp_c < 80

  return (
    <div className="flex flex-col h-full fade-up">
      <div className="flex items-center justify-between mb-4">
        <button onClick={onBack} className="text-dim text-sm font-light tracking-wide">← Back</button>
        <button
          onClick={onSwitchUnit}
          disabled={!onSwitchUnit}
          className="text-sm font-light text-mute tracking-wide flex items-center gap-1"
        >
          {unit.name}
          {onSwitchUnit && (
            <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" className="text-dim">
              <path d="M6 9l6 6 6-6" />
            </svg>
          )}
        </button>
        <div className="w-12" />
      </div>

      <div className="flex-1 overflow-y-auto no-scrollbar space-y-3 -mx-1 px-1 pb-4">
        {/* Unit Status Banner */}
        <div
          className="rounded-2xl p-5 text-center fade-up"
          style={{
            background: status.level === 'critical'
              ? 'rgba(239, 68, 68, 0.08)'
              : status.level === 'warning'
                ? 'rgba(251, 191, 36, 0.08)'
                : 'rgba(62, 166, 255, 0.06)',
            border: `1px solid ${status.level === 'critical'
              ? 'rgba(239, 68, 68, 0.2)'
              : status.level === 'warning'
                ? 'rgba(251, 191, 36, 0.2)'
                : 'rgba(62, 166, 255, 0.12)'}`,
          }}
        >
          <div className="flex items-center justify-center gap-2 mb-1">
            <div className={`w-2.5 h-2.5 rounded-full ${
              status.level === 'critical' ? 'bg-red-500' :
              status.level === 'warning' ? 'bg-yellow-400' : 'bg-accent'
            }`} />
            <div className={`text-lg font-medium ${
              status.level === 'critical' ? 'text-red-400' :
              status.level === 'warning' ? 'text-yellow-300' : 'text-accent'
            }`}>
              Unit Status: {status.label}
            </div>
          </div>
          <div className="text-xs text-dim font-light">
            {alerts.length === 0
              ? 'All systems operating normally'
              : `${unitErrors.length > 0 ? `${unitErrors.length} error${unitErrors.length > 1 ? 's' : ''}` : ''}${unitErrors.length > 0 && diagnosticAlerts.length > 0 ? ' · ' : ''}${diagnosticAlerts.length > 0 ? `${diagnosticAlerts.length} maintenance alert${diagnosticAlerts.length > 1 ? 's' : ''}` : ''}`
            }
          </div>
        </div>

        {/* Unit Errors */}
        {unitErrors.length > 0 && (
          <div className="space-y-2 fade-up fade-up-delay-1">
            <div className="text-xs uppercase tracking-widest text-dim font-light px-1">Unit Errors</div>
            {unitErrors.map(a => <AlertCard key={a.id} alert={a} />)}
          </div>
        )}

        {/* Maintenance Warnings */}
        {diagnosticAlerts.length > 0 && (
          <div className="space-y-2 fade-up fade-up-delay-1">
            <div className="text-xs uppercase tracking-widest text-dim font-light px-1">Maintenance Alerts</div>
            {diagnosticAlerts.map(a => <AlertCard key={a.id} alert={a} />)}
          </div>
        )}

        {/* Indoor */}
        <div className="glass rounded-2xl p-4 fade-up fade-up-delay-2">
          <div className="text-xs uppercase tracking-widest text-dim font-light mb-3">Indoor Unit</div>
          <div className="space-y-2">
            <StatRow label="Power" value={state.power ? 'On' : 'Off'} highlight={state.power} />
            <StatRow label="Mode" value={modeLabel} />
            <StatRow label="Fan speed" value={state.mute ? 'Quiet' : state.turbo ? 'Turbo' : fanLabel} />
            <StatRow label="Room temp" value={safeF(state.indoor_c)} />
            <StatRow label="Setpoint" value={safeF(state.setpoint_c)} />
            <StatRow label="Coil temp" value={safeF(state.indoor_coil_c)} sub="Evaporator" />
            <StatRow label="Fan RPM" value={state.indoor_fan_speed === 0 ? 'Off' : safeNum(state.indoor_fan_speed)} sub="Actual" />
            <StatRow label="Eco" value={state.eco ? 'On' : 'Off'} />
            <StatRow label="Swing" value={state.vswing_pos === 0 ? 'Off' : state.vswing_pos === 6 ? 'Sweep' : `Position ${state.vswing_pos}`} />
          </div>
        </div>

        {/* Outdoor */}
        <div className="glass rounded-2xl p-4 fade-up fade-up-delay-3">
          <div className="text-xs uppercase tracking-widest text-dim font-light mb-3">Outdoor Unit</div>
          {!hasOutdoorData ? (
            <div className="text-sm text-dim font-light text-center py-2">
              No outdoor unit data — unit may be disconnected or not communicating
            </div>
          ) : (
            <div className="space-y-2">
              <StatRow label="Ambient temp" value={safeF(state.outdoor_temp_c)} />
              <StatRow label="Condenser coil" value={safeF(state.condenser_coil_c)} />
              <StatRow label="Discharge temp" value={safeF(state.discharge_temp_c)} sub="Compressor pipe" />
              <StatRow label="Fan speed" value={state.outdoor_fan_speed === 0 ? 'Off' : safeNum(state.outdoor_fan_speed)} />
            </div>
          )}
        </div>

        {/* Compressor */}
        <div className="glass rounded-2xl p-4 fade-up fade-up-delay-4">
          <div className="text-xs uppercase tracking-widest text-dim font-light mb-3">Compressor</div>
          <div className="space-y-2">
            <StatRow label="Status" value={state.compressor_running ? 'Running' : 'Stopped'} highlight={state.compressor_running} />
            <StatRow label="Frequency" value={state.compressor_hz > 0 ? `${state.compressor_hz} Hz` : 'Off'} />
            <StatRow label="Reversing valve" value={state.four_way_valve ? 'Heating' : 'Cooling'} />
            <StatRow label="Antifreeze" value={state.antifreeze ? 'Active' : 'Off'} warn={state.antifreeze} />
          </div>
        </div>

        {/* Power */}
        <div className="glass rounded-2xl p-4">
          <div className="text-xs uppercase tracking-widest text-dim font-light mb-3">Power & Maintenance</div>
          <div className="space-y-2">
            <StatRow label="Voltage" value={safeNum(state.supply_voltage_raw)} sub="Raw reading" />
            <StatRow label="Current" value={safeNum(state.current_draw_raw)} sub="Raw reading" />
            <StatRow label="Filter" value={state.filter_alert ? 'Needs cleaning' : 'OK'}
              highlight={!state.filter_alert} warn={state.filter_alert} />
            <StatRow label="Display" value={state.display ? 'On' : 'Off'} />
            <StatRow label="Beep" value={state.beep ? 'On' : 'Off'} />
          </div>
        </div>
      </div>
    </div>
  )
}

function AlertCard({ alert }: { alert: Alert }) {
  return (
    <div
      className="rounded-2xl p-4"
      style={{
        background: alert.level === 'critical'
          ? 'rgba(239, 68, 68, 0.1)'
          : 'rgba(251, 191, 36, 0.1)',
        border: `1px solid ${alert.level === 'critical'
          ? 'rgba(239, 68, 68, 0.25)'
          : 'rgba(251, 191, 36, 0.25)'}`,
      }}
    >
      <div className="flex items-center gap-2 mb-1">
        <div className={`w-2 h-2 rounded-full ${alert.level === 'critical' ? 'bg-red-500' : 'bg-yellow-400'}`} />
        <div className={`text-sm font-medium ${alert.level === 'critical' ? 'text-red-400' : 'text-yellow-300'}`}>
          {alert.title}
        </div>
        {alert.code && (
          <span className="ml-auto text-xs font-mono text-dim">{alert.code}</span>
        )}
      </div>
      <div className="text-xs font-light text-mute leading-relaxed">{alert.detail}</div>
    </div>
  )
}

function StatRow({ label, value, sub, highlight, warn }: {
  label: string; value: string; sub?: string; highlight?: boolean; warn?: boolean
}) {
  return (
    <div className="flex items-center justify-between py-0.5">
      <div>
        <div className="text-sm font-light text-mute">{label}</div>
        {sub && <div className="text-xs text-dim font-light">{sub}</div>}
      </div>
      <div className={`text-sm tabular font-light ${warn ? 'text-yellow-300' : highlight ? 'text-accent' : ''}`}>
        {value}
      </div>
    </div>
  )
}
