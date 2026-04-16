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
