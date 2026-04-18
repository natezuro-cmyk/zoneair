import { describe, it, expect, vi, beforeEach } from 'vitest'
import { UnitClient } from './unitClient'
import type { AcState } from './unitClient'

// A complete mock AcState for testing
const mockState: AcState = {
  online: true,
  power: true,
  mode: 1,       // Cool
  fan: 3,        // Mid
  setpoint_c: 22,
  indoor_c: 20,
  eco: false,
  turbo: false,
  mute: false,
  vswing_pos: 0,
  display: true,
  beep: true,
  indoor_coil_c: 18,
  outdoor_temp_c: 35,
  condenser_coil_c: 40,
  discharge_temp_c: 55,
  compressor_hz: 60,
  outdoor_fan_speed: 3,
  indoor_fan_speed: 4,
  compressor_running: true,
  four_way_valve: false,
  antifreeze: false,
  filter_alert: false,
  supply_voltage_raw: 220,
  current_draw_raw: 5,
  error_code1: 0,
  error_code2: 0,
}

describe('UnitClient', () => {
  let client: UnitClient

  beforeEach(() => {
    client = new UnitClient('192.168.1.100')
    vi.restoreAllMocks()
  })

  // ── base URL ────────────────────────────────────────────────────────────────

  describe('base()', () => {
    it('returns http://<host>', () => {
      expect(client.base()).toBe('http://192.168.1.100')
    })

    it('works with a .local mDNS hostname', () => {
      const c = new UnitClient('zoneair-bedroom.local')
      expect(c.base()).toBe('http://zoneair-bedroom.local')
    })
  })

  // ── getState ────────────────────────────────────────────────────────────────

  describe('getState()', () => {
    it('GETs /state and returns parsed JSON', async () => {
      vi.spyOn(globalThis, 'fetch').mockResolvedValueOnce({
        ok: true,
        json: async () => mockState,
      } as Response)

      const result = await client.getState()

      expect(fetch).toHaveBeenCalledWith('http://192.168.1.100/state')
      expect(result).toEqual(mockState)
    })

    it('throws with status code when response is not ok', async () => {
      vi.spyOn(globalThis, 'fetch').mockResolvedValueOnce({
        ok: false,
        status: 503,
      } as Response)

      await expect(client.getState()).rejects.toThrow('state 503')
    })

    it('returns correct mode from state', async () => {
      const heatState = { ...mockState, mode: 2 as const }
      vi.spyOn(globalThis, 'fetch').mockResolvedValueOnce({
        ok: true,
        json: async () => heatState,
      } as Response)

      const result = await client.getState()
      expect(result.mode).toBe(2)
    })
  })

  // ── sendCommand ─────────────────────────────────────────────────────────────

  describe('sendCommand()', () => {
    it('POSTs to /command with JSON body', async () => {
      vi.spyOn(globalThis, 'fetch').mockResolvedValueOnce({ ok: true } as Response)

      await client.sendCommand({ mode: 1 })

      expect(fetch).toHaveBeenCalledWith('http://192.168.1.100/command', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ mode: 1 }),
      })
    })

    it('throws with status code when response is not ok', async () => {
      vi.spyOn(globalThis, 'fetch').mockResolvedValueOnce({
        ok: false,
        status: 400,
      } as Response)

      await expect(client.sendCommand({ power: true })).rejects.toThrow('command 400')
    })

    // ── Mode commands ──────────────────────────────────────────────────────────

    it('sends mode: 0 (Off)', async () => {
      vi.spyOn(globalThis, 'fetch').mockResolvedValueOnce({ ok: true } as Response)
      await client.sendCommand({ mode: 0 })
      expect(fetch).toHaveBeenCalledWith(expect.any(String),
        expect.objectContaining({ body: JSON.stringify({ mode: 0 }) }))
    })

    it('sends mode: 1 (Cool)', async () => {
      vi.spyOn(globalThis, 'fetch').mockResolvedValueOnce({ ok: true } as Response)
      await client.sendCommand({ mode: 1 })
      expect(fetch).toHaveBeenCalledWith(expect.any(String),
        expect.objectContaining({ body: JSON.stringify({ mode: 1 }) }))
    })

    it('sends mode: 2 (Heat)', async () => {
      vi.spyOn(globalThis, 'fetch').mockResolvedValueOnce({ ok: true } as Response)
      await client.sendCommand({ mode: 2 })
      expect(fetch).toHaveBeenCalledWith(expect.any(String),
        expect.objectContaining({ body: JSON.stringify({ mode: 2 }) }))
    })

    it('sends mode: 3 (Auto)', async () => {
      vi.spyOn(globalThis, 'fetch').mockResolvedValueOnce({ ok: true } as Response)
      await client.sendCommand({ mode: 3 })
      expect(fetch).toHaveBeenCalledWith(expect.any(String),
        expect.objectContaining({ body: JSON.stringify({ mode: 3 }) }))
    })

    it('sends mode: 4 (Dry)', async () => {
      vi.spyOn(globalThis, 'fetch').mockResolvedValueOnce({ ok: true } as Response)
      await client.sendCommand({ mode: 4 })
      expect(fetch).toHaveBeenCalledWith(expect.any(String),
        expect.objectContaining({ body: JSON.stringify({ mode: 4 }) }))
    })

    it('sends mode: 5 (Fan)', async () => {
      vi.spyOn(globalThis, 'fetch').mockResolvedValueOnce({ ok: true } as Response)
      await client.sendCommand({ mode: 5 })
      expect(fetch).toHaveBeenCalledWith(expect.any(String),
        expect.objectContaining({ body: JSON.stringify({ mode: 5 }) }))
    })

    // ── Power commands ─────────────────────────────────────────────────────────

    it('sends power: true', async () => {
      vi.spyOn(globalThis, 'fetch').mockResolvedValueOnce({ ok: true } as Response)
      await client.sendCommand({ power: true })
      expect(fetch).toHaveBeenCalledWith(expect.any(String),
        expect.objectContaining({ body: JSON.stringify({ power: true }) }))
    })

    it('sends power: false', async () => {
      vi.spyOn(globalThis, 'fetch').mockResolvedValueOnce({ ok: true } as Response)
      await client.sendCommand({ power: false })
      expect(fetch).toHaveBeenCalledWith(expect.any(String),
        expect.objectContaining({ body: JSON.stringify({ power: false }) }))
    })

    // ── Temperature commands ───────────────────────────────────────────────────

    it('sends setpoint_c', async () => {
      vi.spyOn(globalThis, 'fetch').mockResolvedValueOnce({ ok: true } as Response)
      await client.sendCommand({ setpoint_c: 24 })
      expect(fetch).toHaveBeenCalledWith(expect.any(String),
        expect.objectContaining({ body: JSON.stringify({ setpoint_c: 24 }) }))
    })

    it('sends setpoint_f (Fahrenheit path)', async () => {
      vi.spyOn(globalThis, 'fetch').mockResolvedValueOnce({ ok: true } as Response)
      await client.sendCommand({ setpoint_f: 72 })
      expect(fetch).toHaveBeenCalledWith(expect.any(String),
        expect.objectContaining({ body: JSON.stringify({ setpoint_f: 72 }) }))
    })

    // ── Fan commands ───────────────────────────────────────────────────────────

    it('sends turbo: true, mute: false for Turbo fan', async () => {
      vi.spyOn(globalThis, 'fetch').mockResolvedValueOnce({ ok: true } as Response)
      await client.sendCommand({ turbo: true, mute: false })
      expect(fetch).toHaveBeenCalledWith(expect.any(String),
        expect.objectContaining({ body: JSON.stringify({ turbo: true, mute: false }) }))
    })

    it('sends mute: true, turbo: false for Quiet fan', async () => {
      vi.spyOn(globalThis, 'fetch').mockResolvedValueOnce({ ok: true } as Response)
      await client.sendCommand({ mute: true, turbo: false })
      expect(fetch).toHaveBeenCalledWith(expect.any(String),
        expect.objectContaining({ body: JSON.stringify({ mute: true, turbo: false }) }))
    })

    it('sends compound command (mode + power together)', async () => {
      vi.spyOn(globalThis, 'fetch').mockResolvedValueOnce({ ok: true } as Response)
      await client.sendCommand({ power: true, mode: 1 })
      expect(fetch).toHaveBeenCalledWith(expect.any(String),
        expect.objectContaining({ body: JSON.stringify({ power: true, mode: 1 }) }))
    })
  })

  // ── openSocket ──────────────────────────────────────────────────────────────

  describe('openSocket()', () => {
    it('creates a WebSocket pointing at ws://<host>/ws', () => {
      const MockWS = vi.fn(function() { return { onmessage: null, onclose: null } })
      vi.stubGlobal('WebSocket', MockWS)

      client.openSocket(() => {})

      expect(MockWS).toHaveBeenCalledWith('ws://192.168.1.100/ws')
    })

    it('calls onState with parsed AcState when a message arrives', () => {
      const onState = vi.fn()
      const mockWs = { onmessage: null as any, onclose: null as any }
      vi.stubGlobal('WebSocket', vi.fn(function() { return mockWs }))

      client.openSocket(onState)
      mockWs.onmessage({ data: JSON.stringify(mockState) })

      expect(onState).toHaveBeenCalledWith(mockState)
    })

    it('does NOT call onState when the message is invalid JSON', () => {
      const onState = vi.fn()
      const mockWs = { onmessage: null as any, onclose: null as any }
      vi.stubGlobal('WebSocket', vi.fn(function() { return mockWs }))

      client.openSocket(onState)
      mockWs.onmessage({ data: 'not json at all' })

      expect(onState).not.toHaveBeenCalled()
    })

    it('calls onClose callback when the socket closes', () => {
      const onClose = vi.fn()
      const mockWs = { onmessage: null as any, onclose: null as any }
      vi.stubGlobal('WebSocket', vi.fn(function() { return mockWs }))

      client.openSocket(() => {}, onClose)
      mockWs.onclose()

      expect(onClose).toHaveBeenCalled()
    })

    it('returns the WebSocket instance', () => {
      const mockWsInstance = { onmessage: null, onclose: null }
      vi.stubGlobal('WebSocket', vi.fn(function() { return mockWsInstance }))

      const ws = client.openSocket(() => {})

      expect(ws).toBe(mockWsInstance)
    })
  })
})
