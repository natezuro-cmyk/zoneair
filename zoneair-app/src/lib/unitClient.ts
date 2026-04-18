export type Mode = 0 | 1 | 2 | 3 | 4 | 5
export type Fan = 0 | 1 | 2 | 3 | 4 | 5
export type VSwing = 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8

export type AcState = {
  online: boolean
  power: boolean
  mode: Mode
  fan: Fan
  setpoint_c: number
  indoor_c: number
  eco: boolean
  turbo: boolean
  mute: boolean
  vswing_pos: VSwing
  display: boolean
  beep: boolean
  // Extended diagnostics
  indoor_coil_c: number
  outdoor_temp_c: number
  condenser_coil_c: number
  discharge_temp_c: number
  compressor_hz: number
  outdoor_fan_speed: number
  indoor_fan_speed: number
  compressor_running: boolean
  four_way_valve: boolean
  antifreeze: boolean
  filter_alert: boolean
  supply_voltage_raw: number
  current_draw_raw: number
  // Error code bytes (from frame bytes 20-29, only non-zero during faults)
  error_code1: number
  error_code2: number
}

export type Command = Partial<Pick<AcState,
  'power' | 'mode' | 'fan' | 'setpoint_c' | 'eco' | 'turbo' | 'mute' | 'vswing_pos' | 'display' | 'beep'
>> & { setpoint_f?: number }

export class UnitClient {
  host: string
  constructor(host: string) { this.host = host }
  base() { return `http://${this.host}` }

  async getState(): Promise<AcState> {
    const r = await fetch(`${this.base()}/state`)
    if (!r.ok) throw new Error(`state ${r.status}`)
    return r.json()
  }

  async sendCommand(c: Command): Promise<void> {
    const r = await fetch(`${this.base()}/command`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(c),
    })
    if (!r.ok) throw new Error(`command ${r.status}`)
  }

  openSocket(onState: (s: AcState) => void, onClose?: () => void): WebSocket {
    const ws = new WebSocket(`ws://${this.host}/ws`)
    ws.onmessage = (e) => { try { onState(JSON.parse(e.data)) } catch {} }
    if (onClose) ws.onclose = onClose
    return ws
  }
}
