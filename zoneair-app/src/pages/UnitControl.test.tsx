import { describe, it, expect, vi, beforeEach } from 'vitest'
import { render, screen, fireEvent, waitFor, act } from '@testing-library/react'
import UnitControl from './UnitControl'
import { UnitClient } from '../lib/unitClient'
import type { AcState } from '../lib/unitClient'

// ── Mocks ────────────────────────────────────────────────────────────────────

// Mock the Zustand units store so tests don't touch localStorage
vi.mock('../state/units', () => ({
  useUnits: (selector: any) =>
    selector({ remove: vi.fn(), rename: vi.fn() }),
}))

// Mock analyzeState so we don't need to worry about the alerts logic
vi.mock('../lib/alerts', () => ({
  analyzeState: () => [],
}))

// ── Helpers ──────────────────────────────────────────────────────────────────

const mockUnit = { id: 'test-unit', name: 'Test AC', host: '192.168.1.100' }

/** Build a full AcState with sensible defaults, override what you need. */
function makeState(overrides: Partial<AcState> = {}): AcState {
  return {
    online: true,
    power: true,
    mode: 1,        // Cool
    fan: 3,         // Mid (maps to 'med')
    setpoint_c: 22, // 72 °F
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
    ...overrides,
  }
}

/**
 * Render UnitControl, resolve the initial getState fetch, and wait for the
 * loading spinner to disappear before returning.
 */
async function renderWithState(stateOverrides: Partial<AcState> = {}) {
  const state = makeState(stateOverrides)

  const mockFetch = vi.fn()
  // First call: getState() — returns our mock state
  mockFetch.mockResolvedValueOnce({ ok: true, json: async () => state })
  vi.stubGlobal('fetch', mockFetch)

  // Spy on openSocket directly so we can capture and trigger the callbacks
  let capturedOnState: ((s: AcState) => void) | null = null
  let capturedOnClose: (() => void) | null = null
  const mockClose = vi.fn()
  vi.spyOn(UnitClient.prototype, 'openSocket').mockImplementation(function(onState, onClose) {
    capturedOnState = onState
    capturedOnClose = onClose ?? null
    return { close: mockClose } as unknown as WebSocket
  })

  const onBack = vi.fn()
  const onDetails = vi.fn()

  render(<UnitControl unit={mockUnit} onBack={onBack} onDetails={onDetails} />)

  // Wait for loading to finish
  await waitFor(() => {
    expect(screen.queryByText(/Connecting to/)).not.toBeInTheDocument()
  })

  // mockWs helpers to simulate device messages and connection events
  const mockWs = {
    close: mockClose,
    onmessage: (event: { data: string }) => {
      try { if (capturedOnState) capturedOnState(JSON.parse(event.data)) } catch {}
    },
    triggerClose: () => { if (capturedOnClose) capturedOnClose() },
  }

  return { state, mockFetch, mockWs, onBack, onDetails }
}

// ── Tests ────────────────────────────────────────────────────────────────────

describe('UnitControl', () => {
  beforeEach(() => {
    vi.restoreAllMocks()
  })

  // ── Loading state ──────────────────────────────────────────────────────────

  describe('Loading state', () => {
    it('shows "Connecting to <name>" before state resolves', () => {
      vi.stubGlobal('fetch', vi.fn().mockImplementation(() => new Promise(() => {})))
      vi.stubGlobal('WebSocket', vi.fn().mockImplementation(() => ({
        onmessage: null, onclose: null, close: vi.fn(),
      })))

      render(<UnitControl unit={mockUnit} onBack={() => {}} onDetails={() => {}} />)

      expect(screen.getByText(/Connecting to Test AC/)).toBeInTheDocument()
    })
  })

  // ── Mode switching ─────────────────────────────────────────────────────────

  describe('Mode switching', () => {
    it('renders all 5 mode buttons (Cool, Heat, Auto, Dry, Fan)', async () => {
      await renderWithState()

      expect(screen.getByRole('button', { name: 'Cool' })).toBeInTheDocument()
      expect(screen.getByRole('button', { name: 'Heat' })).toBeInTheDocument()
      expect(screen.getByRole('button', { name: 'Auto' })).toBeInTheDocument()
      expect(screen.getByRole('button', { name: 'Dry' })).toBeInTheDocument()
      expect(screen.getByRole('button', { name: 'Fan' })).toBeInTheDocument()
    })

    it('highlights the current active mode pill', async () => {
      await renderWithState({ mode: 2 }) // Heat

      const heatBtn = screen.getByText('Heat').closest('button')
      expect(heatBtn).toHaveClass('pill-active')

      // Others should not be active
      expect(screen.getByText('Cool').closest('button')).not.toHaveClass('pill-active')
    })

    it('clicking Cool sends { mode: 1 } to /command', async () => {
      const { mockFetch } = await renderWithState({ mode: 2 }) // start in Heat
      mockFetch.mockResolvedValueOnce({ ok: true })

      fireEvent.click(screen.getByText('Cool'))

      await waitFor(() => {
        expect(mockFetch).toHaveBeenCalledWith(
          'http://192.168.1.100/command',
          expect.objectContaining({ body: JSON.stringify({ mode: 1 }) })
        )
      })
    })

    it('clicking Heat sends { mode: 2 } to /command', async () => {
      const { mockFetch } = await renderWithState({ mode: 1 })
      mockFetch.mockResolvedValueOnce({ ok: true })

      fireEvent.click(screen.getByText('Heat'))

      await waitFor(() => {
        expect(mockFetch).toHaveBeenCalledWith(
          'http://192.168.1.100/command',
          expect.objectContaining({ body: JSON.stringify({ mode: 2 }) })
        )
      })
    })

    it('clicking Auto sends { mode: 3 } to /command', async () => {
      const { mockFetch } = await renderWithState({ mode: 1 })
      mockFetch.mockResolvedValueOnce({ ok: true })

      fireEvent.click(screen.getByText('Auto'))

      await waitFor(() => {
        expect(mockFetch).toHaveBeenCalledWith(
          'http://192.168.1.100/command',
          expect.objectContaining({ body: JSON.stringify({ mode: 3 }) })
        )
      })
    })

    it('clicking Dry sends { mode: 4 } to /command', async () => {
      const { mockFetch } = await renderWithState({ mode: 1 })
      mockFetch.mockResolvedValueOnce({ ok: true })

      fireEvent.click(screen.getByText('Dry'))

      await waitFor(() => {
        expect(mockFetch).toHaveBeenCalledWith(
          'http://192.168.1.100/command',
          expect.objectContaining({ body: JSON.stringify({ mode: 4 }) })
        )
      })
    })

    it('clicking Fan sends { mode: 5 } to /command', async () => {
      const { mockFetch } = await renderWithState({ mode: 1 })
      mockFetch.mockResolvedValueOnce({ ok: true })

      fireEvent.click(screen.getByRole('button', { name: 'Fan' }))

      await waitFor(() => {
        expect(mockFetch).toHaveBeenCalledWith(
          'http://192.168.1.100/command',
          expect.objectContaining({ body: JSON.stringify({ mode: 5 }) })
        )
      })
    })

    it('optimistically highlights the new mode immediately (before server reply)', async () => {
      // Fetch for the command hangs — we still expect the UI to update
      const { mockFetch } = await renderWithState({ mode: 1 }) // Cool
      mockFetch.mockImplementationOnce(() => new Promise(() => {}))

      fireEvent.click(screen.getByText('Heat'))

      // Should be highlighted without waiting for server
      expect(screen.getByText('Heat').closest('button')).toHaveClass('pill-active')
      expect(screen.getByText('Cool').closest('button')).not.toHaveClass('pill-active')
    })

    it('shows the mode label in the temperature display', async () => {
      await renderWithState({ power: true, mode: 2 }) // Heat

      // The label "· Heat" should appear under the temperature
      expect(screen.getByText('· Heat')).toBeInTheDocument()
    })
  })

  // ── Power toggle ───────────────────────────────────────────────────────────

  describe('Power toggle', () => {
    it('shows — for temperature when unit is off', async () => {
      await renderWithState({ power: false })
      // The temperature display renders an em-dash when powered off
      expect(screen.getByText('—')).toBeInTheDocument()
    })

    it('sends { power: false } when toggled off', async () => {
      const { mockFetch } = await renderWithState({ power: true })
      mockFetch.mockResolvedValueOnce({ ok: true })

      const powerToggle = document.querySelector('.toggle-track') as HTMLElement
      fireEvent.click(powerToggle)

      await waitFor(() => {
        expect(mockFetch).toHaveBeenCalledWith(
          'http://192.168.1.100/command',
          expect.objectContaining({ body: JSON.stringify({ power: false }) })
        )
      })
    })

    it('sends { power: true, display: true } when toggled on', async () => {
      const { mockFetch } = await renderWithState({ power: false })
      mockFetch.mockResolvedValueOnce({ ok: true })

      const powerToggle = document.querySelector('.toggle-track') as HTMLElement
      fireEvent.click(powerToggle)

      await waitFor(() => {
        expect(mockFetch).toHaveBeenCalledWith(
          'http://192.168.1.100/command',
          expect.objectContaining({ body: JSON.stringify({ power: true, display: true }) })
        )
      })
    })
  })

  // ── Display flash ─────────────────────────────────────────────────────────

  describe('Display flash', () => {
    it('briefly turns display on when a setting changes and display is off', async () => {
      const { mockFetch } = await renderWithState({ display: false, mode: 1 })
      mockFetch.mockResolvedValue({ ok: true })

      fireEvent.click(screen.getByRole('button', { name: 'Heat' }))

      await waitFor(() => {
        expect(mockFetch).toHaveBeenCalledWith(
          'http://192.168.1.100/command',
          expect.objectContaining({ body: JSON.stringify({ display: true }) })
        )
      })
    })

    it('does not flash display when display is already on', async () => {
      const { mockFetch } = await renderWithState({ display: true, mode: 1 })
      mockFetch.mockResolvedValue({ ok: true })

      fireEvent.click(screen.getByRole('button', { name: 'Heat' }))

      await waitFor(() => {
        expect(mockFetch).toHaveBeenCalledWith(
          'http://192.168.1.100/command',
          expect.objectContaining({ body: JSON.stringify({ mode: 2 }) })
        )
      })

      // display: true should NOT have been sent as a separate command
      const displayOnCalls = mockFetch.mock.calls.filter(
        (c: any[]) => c[1]?.body === JSON.stringify({ display: true })
      )
      expect(displayOnCalls).toHaveLength(0)
    })
  })

  // ── Reconnecting banner ────────────────────────────────────────────────────

  describe('Reconnecting banner', () => {
    it('does not show "Reconnecting…" after initial load', async () => {
      await renderWithState()
      expect(screen.queryByText('Reconnecting…')).not.toBeInTheDocument()
    })

    it('shows "Reconnecting…" when the WebSocket closes', async () => {
      const { mockWs } = await renderWithState()

      act(() => { mockWs.triggerClose() })

      expect(screen.getByText('Reconnecting…')).toBeInTheDocument()
    })
  })

  // ── Fan switching ──────────────────────────────────────────────────────────

  describe('Fan switching', () => {
    it('renders all 5 fan choices', async () => {
      await renderWithState()

      expect(screen.getByText('Quiet')).toBeInTheDocument()
      expect(screen.getByText('Low')).toBeInTheDocument()
      expect(screen.getByText('Med')).toBeInTheDocument()
      expect(screen.getByText('High')).toBeInTheDocument()
      expect(screen.getByText('Turbo')).toBeInTheDocument()
    })

    it('highlights "Med" when fan=3 and turbo/mute are false', async () => {
      await renderWithState({ fan: 3, turbo: false, mute: false })
      expect(screen.getByText('Med').closest('button')).toHaveClass('pill-active')
    })

    it('highlights "Turbo" when turbo=true', async () => {
      await renderWithState({ turbo: true, mute: false })
      expect(screen.getByText('Turbo').closest('button')).toHaveClass('pill-active')
    })

    it('highlights "Quiet" when mute=true', async () => {
      await renderWithState({ mute: true, turbo: false })
      expect(screen.getByText('Quiet').closest('button')).toHaveClass('pill-active')
    })

    it('clicking Turbo sends { turbo: true, mute: false }', async () => {
      const { mockFetch } = await renderWithState({ fan: 3, turbo: false, mute: false })
      mockFetch.mockResolvedValueOnce({ ok: true })

      fireEvent.click(screen.getByText('Turbo'))

      await waitFor(() => {
        expect(mockFetch).toHaveBeenCalledWith(
          'http://192.168.1.100/command',
          expect.objectContaining({ body: JSON.stringify({ turbo: true, mute: false }) })
        )
      })
    })

    it('clicking Quiet sends { mute: true, turbo: false }', async () => {
      const { mockFetch } = await renderWithState({ fan: 3, turbo: false, mute: false })
      mockFetch.mockResolvedValueOnce({ ok: true })

      fireEvent.click(screen.getByText('Quiet'))

      await waitFor(() => {
        expect(mockFetch).toHaveBeenCalledWith(
          'http://192.168.1.100/command',
          expect.objectContaining({ body: JSON.stringify({ mute: true, turbo: false }) })
        )
      })
    })

    it('clicking Med sends { fan: 3, turbo: false, mute: false }', async () => {
      const { mockFetch } = await renderWithState({ fan: 2, turbo: false, mute: false }) // start in Low
      mockFetch.mockResolvedValueOnce({ ok: true })

      fireEvent.click(screen.getByText('Med'))

      await waitFor(() => {
        expect(mockFetch).toHaveBeenCalledWith(
          'http://192.168.1.100/command',
          expect.objectContaining({ body: JSON.stringify({ fan: 3, turbo: false, mute: false }) })
        )
      })
    })

    it('clicking Low sends { fan: 2, turbo: false, mute: false }', async () => {
      const { mockFetch } = await renderWithState({ fan: 3, turbo: false, mute: false })
      mockFetch.mockResolvedValueOnce({ ok: true })

      fireEvent.click(screen.getByText('Low'))

      await waitFor(() => {
        expect(mockFetch).toHaveBeenCalledWith(
          'http://192.168.1.100/command',
          expect.objectContaining({ body: JSON.stringify({ fan: 2, turbo: false, mute: false }) })
        )
      })
    })

    it('clicking High sends { fan: 5, turbo: false, mute: false }', async () => {
      const { mockFetch } = await renderWithState({ fan: 3, turbo: false, mute: false })
      mockFetch.mockResolvedValueOnce({ ok: true })

      fireEvent.click(screen.getByText('High'))

      await waitFor(() => {
        expect(mockFetch).toHaveBeenCalledWith(
          'http://192.168.1.100/command',
          expect.objectContaining({ body: JSON.stringify({ fan: 5, turbo: false, mute: false }) })
        )
      })
    })
  })

  // ── Temperature ────────────────────────────────────────────────────────────

  describe('Temperature display', () => {
    it('displays the setpoint in Fahrenheit (22°C = 72°F)', async () => {
      await renderWithState({ setpoint_c: 22 })
      expect(screen.getByText('72')).toBeInTheDocument()
    })

    it('shows indoor temperature in Fahrenheit', async () => {
      await renderWithState({ indoor_c: 20 }) // 20°C = 68°F
      expect(screen.getByText(/68° inside/)).toBeInTheDocument()
    })

    it('+ button sends incremented setpoint_f', async () => {
      const { mockFetch } = await renderWithState({ setpoint_c: 22 }) // 72°F
      mockFetch.mockResolvedValueOnce({ ok: true })

      fireEvent.click(screen.getByText('+'))

      await waitFor(() => {
        expect(mockFetch).toHaveBeenCalledWith(
          'http://192.168.1.100/command',
          expect.objectContaining({ body: JSON.stringify({ setpoint_f: 73 }) })
        )
      })
    })

    it('− button sends decremented setpoint_f', async () => {
      const { mockFetch } = await renderWithState({ setpoint_c: 22 }) // 72°F
      mockFetch.mockResolvedValueOnce({ ok: true })

      fireEvent.click(screen.getByText('−'))

      await waitFor(() => {
        expect(mockFetch).toHaveBeenCalledWith(
          'http://192.168.1.100/command',
          expect.objectContaining({ body: JSON.stringify({ setpoint_f: 71 }) })
        )
      })
    })

    it('clamps temperature at 88°F max', async () => {
      const { mockFetch } = await renderWithState({ setpoint_c: 31.1 }) // ≈ 88°F
      mockFetch.mockResolvedValueOnce({ ok: true })

      fireEvent.click(screen.getByText('+'))

      await waitFor(() => {
        // Should stay at 88, not go to 89
        expect(mockFetch).toHaveBeenCalledWith(
          'http://192.168.1.100/command',
          expect.objectContaining({ body: JSON.stringify({ setpoint_f: 88 }) })
        )
      })
    })

    it('clamps temperature at 63°F min', async () => {
      const { mockFetch } = await renderWithState({ setpoint_c: 17.2 }) // ≈ 63°F
      mockFetch.mockResolvedValueOnce({ ok: true })

      fireEvent.click(screen.getByText('−'))

      await waitFor(() => {
        // Should stay at 63, not go to 62
        expect(mockFetch).toHaveBeenCalledWith(
          'http://192.168.1.100/command',
          expect.objectContaining({ body: JSON.stringify({ setpoint_f: 63 }) })
        )
      })
    })
  })

  // ── Eco toggle ─────────────────────────────────────────────────────────────

  describe('Eco toggle', () => {
    it('sends { eco: true } when toggling eco on', async () => {
      const { mockFetch } = await renderWithState({ eco: false })
      mockFetch.mockResolvedValueOnce({ ok: true })

      // Eco is the 5th toggle-track (Power, Eco, Display, Beep)
      const toggles = document.querySelectorAll('.toggle-track')
      // Power=0, Eco=1, Display=2, Beep=3
      fireEvent.click(toggles[1]) // Eco

      await waitFor(() => {
        expect(mockFetch).toHaveBeenCalledWith(
          'http://192.168.1.100/command',
          expect.objectContaining({ body: JSON.stringify({ eco: true }) })
        )
      })
    })
  })

  // ── WebSocket updates ──────────────────────────────────────────────────────

  describe('WebSocket state updates', () => {
    it('updates the active mode pill when the server pushes a new mode', async () => {
      const { state, mockWs } = await renderWithState({ mode: 1 }) // Cool

      act(() => {
        mockWs.onmessage({ data: JSON.stringify({ ...state, mode: 2 }) }) // Heat
      })

      await waitFor(() => {
        expect(screen.getByText('Heat').closest('button')).toHaveClass('pill-active')
        expect(screen.getByText('Cool').closest('button')).not.toHaveClass('pill-active')
      })
    })

    it('updates the temperature display when the server pushes a new setpoint', async () => {
      const { state, mockWs } = await renderWithState({ setpoint_c: 22 }) // 72°F

      act(() => {
        mockWs.onmessage({ data: JSON.stringify({ ...state, setpoint_c: 24 }) }) // 75°F
      })

      await waitFor(() => {
        expect(screen.getByText('75')).toBeInTheDocument()
      })
    })

    it('ignores malformed WebSocket messages', async () => {
      const { mockWs } = await renderWithState({ mode: 1 })

      act(() => {
        mockWs.onmessage({ data: 'bad json' })
      })

      // Mode should still be Cool
      expect(screen.getByText('Cool').closest('button')).toHaveClass('pill-active')
    })
  })

  // ── Navigation ─────────────────────────────────────────────────────────────

  describe('Navigation', () => {
    it('shows the unit name in the header', async () => {
      await renderWithState()
      expect(screen.getByText('Test AC')).toBeInTheDocument()
    })

    it('calls onBack when ← Back is clicked', async () => {
      const { onBack } = await renderWithState()
      fireEvent.click(screen.getByText(/← Back/))
      expect(onBack).toHaveBeenCalled()
    })
  })
})
