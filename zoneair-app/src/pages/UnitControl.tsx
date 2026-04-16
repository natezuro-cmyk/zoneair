import { useEffect, useRef, useState } from 'react'
import type { Unit } from '../state/units'
import { useUnits } from '../state/units'
import { UnitClient } from '../lib/unitClient'
import type { AcState, Mode, Fan, VSwing } from '../lib/unitClient'

type Props = { unit: Unit; onBack: () => void }

const MODES: [Mode, string][] = [[1,'Cool'],[2,'Heat'],[3,'Auto'],[4,'Dry'],[5,'Fan']]
type FanChoice = 'quiet' | 'low' | 'med' | 'high' | 'turbo'
const FAN_CHOICES: [FanChoice, string][] = [
  ['quiet','Quiet'], ['low','Low'], ['med','Med'], ['high','High'], ['turbo','Turbo'],
]
const FAN_TO_NUMERIC: Record<Exclude<FanChoice,'quiet'|'turbo'>, Fan> = { low: 2, med: 3, high: 5 }
function currentFanChoice(s: AcState): FanChoice {
  if (s.turbo) return 'turbo'
  if (s.mute)  return 'quiet'
  if (s.fan <= 2) return 'low'
  if (s.fan === 3) return 'med'
  return 'high'
}
const SWINGS: [VSwing, string][] = [
  [0,'Off'], [1,'Up'], [3,'Mid'], [5,'Down'], [6,'Sweep'],
]

const cToF = (c: number) => Math.round(c * 9/5 + 32)

export default function UnitControl({ unit, onBack }: Props) {
  const removeUnit = useUnits(s => s.remove)
  const [client] = useState(() => new UnitClient(unit.host))
  const handleRemove = () => {
    if (confirm(`Remove "${unit.name}" from this app? The unit will keep its WiFi setup; this just removes it from your list.`)) {
      removeUnit(unit.id)
      onBack()
    }
  }
  const [state, setState] = useState<AcState | null>(null)
  const [pending, setPending] = useState<Partial<AcState>>({})
  const [offAt, setOffAt] = useState<number | null>(null)
  const [onAt,  setOnAt]  = useState<number | null>(null)
  const [, tick] = useState(0)
  useEffect(() => {
    if (!offAt && !onAt) return
    const i = setInterval(() => {
      tick(x => x + 1)
      const now = Date.now()
      if (offAt && now >= offAt) { client.sendCommand({ power: false }).catch(() => {}); setOffAt(null) }
      if (onAt  && now >= onAt)  { client.sendCommand({ power: true  }).catch(() => {}); setOnAt(null) }
    }, 1000)
    return () => clearInterval(i)
  }, [offAt, onAt, client])

  useEffect(() => {
    let alive = true
    let ws: WebSocket | null = null
    // One-time HTTP fetch to populate initial state, then rely solely on the
    // WebSocket. Polling-while-pending was racing the optimistic UI.
    client.getState().then(s => { if (alive) setState(s) }).catch(() => {})
    try { ws = client.openSocket(s => { if (alive) setState(s) }) } catch {}
    return () => { alive = false; ws?.close() }
  }, [client])

  const view = { ...(state || {}), ...pending } as AcState

  const send = (patch: Partial<AcState>) => {
    setPending(p => ({ ...p, ...patch }))
    client.sendCommand(patch as any).catch(() => {})
  }

  // Smarter pending merge: when state arrives, only clear pending fields whose
  // value now matches the incoming state (so an in-flight toggle doesn't flicker).
  useEffect(() => {
    if (!state) return
    setPending(prev => {
      const next: Partial<AcState> = {}
      for (const k of Object.keys(prev) as (keyof AcState)[]) {
        if (prev[k] !== (state as any)[k]) (next as any)[k] = prev[k]
      }
      return next
    })
  }, [state])

  // Single source of truth for the user's F target. null = no pending change.
  const [targetF, setTargetF] = useState<number | null>(null)
  const sendTimerRef = useRef<number | null>(null)
  useEffect(() => {
    if (state && targetF !== null && cToF(state.setpoint_c) === targetF) {
      setTargetF(null)
    }
  }, [state?.setpoint_c, targetF])

  // Same pattern for power.
  const [targetPower, setTargetPower] = useState<boolean | null>(null)
  useEffect(() => {
    if (state && targetPower !== null && state.power === targetPower) {
      setTargetPower(null)
    }
  }, [state?.power, targetPower])

  if (!state) return (
    <div className="text-mute text-center mt-16">
      Connecting to {unit.name}…
      <button onClick={onBack} className="block mx-auto mt-6 text-sm text-mute underline">Back</button>
    </div>
  )

  const setF = targetF ?? cToF(view.setpoint_c)
  const indoorF = cToF(view.indoor_c)
  const clampF = (f: number) => Math.min(88, Math.max(63, f))  // AC floor is 63°F

  // Debounced send: rapid +/- clicks coalesce into one POST after 250ms idle.
  const stepF = (delta: number) => {
    const next = clampF(setF + delta)
    setTargetF(next)
    if (sendTimerRef.current) window.clearTimeout(sendTimerRef.current)
    sendTimerRef.current = window.setTimeout(() => {
      client.sendCommand({ setpoint_f: next } as any).catch(() => {})
      sendTimerRef.current = null
    }, 250)
  }

  return (
    <div>
      <button onClick={onBack} className="text-mute text-sm mb-4">← Units</button>
      <div className="text-sm text-mute">{unit.name}</div>

      <div className="mt-6 flex items-center justify-center gap-6">
        <button
          onClick={() => stepF(-1)}
          className="w-12 h-12 rounded-full bg-line/60 text-3xl leading-none flex items-center justify-center hover:bg-line transition"
        >−</button>
        <div className="text-7xl font-light tabular-nums leading-none">{setF}°</div>
        <button
          onClick={() => stepF(+1)}
          className="w-12 h-12 rounded-full bg-line/60 text-3xl leading-none flex items-center justify-center hover:bg-line transition"
        >+</button>
      </div>
      <div className="text-center text-xs text-mute mt-3 uppercase tracking-widest">
        Indoor {indoorF}°
      </div>

      <div className="mt-8 bg-line/40 rounded-2xl p-5 space-y-2">
        <Row label="Power">
          <Toggle
            on={targetPower ?? view.power}
            onChange={(v) => { setTargetPower(v); client.sendCommand({ power: v }).catch(() => {}) }}
          />
        </Row>
        <Divider />
        <Row label="Mode"><Pills options={MODES} value={view.mode} onChange={(v) => send({ mode: v })} /></Row>
        <Divider />
        <Row label="Fan">
          <Pills
            options={FAN_CHOICES}
            value={currentFanChoice(view)}
            onChange={(v) => {
              if (v === 'turbo')      send({ turbo: true,  mute: false })
              else if (v === 'quiet') send({ mute: true,   turbo: false })
              else                    send({ fan: FAN_TO_NUMERIC[v], turbo: false, mute: false })
            }}
          />
        </Row>
        <Divider />
        <Row label="Eco"><Toggle on={view.eco}     onChange={(v) => send({ eco: v })} /></Row>
        <Row label="Display"><Toggle on={view.display} onChange={(v) => send({ display: v })} /></Row>
        <Row label="Beep"><Toggle on={view.beep}    onChange={(v) => send({ beep: v })} /></Row>
        <Divider />
        <Row label="Swing"><Pills options={SWINGS} value={view.vswing_pos} onChange={(v) => send({ vswing_pos: v })} /></Row>
        <Divider />
        <Row label="Off in">
          <TimerPills
            timerEnd={offAt}
            onSet={(h) => {
              const end = h === 0 ? null : Date.now() + h * 3600 * 1000
              setOffAt(end)
              // App-side timer only — no firmware timer fields.
            }}
          />
        </Row>
        <Row label="On in">
          <TimerPills
            timerEnd={onAt}
            onSet={(h) => {
              const end = h === 0 ? null : Date.now() + h * 3600 * 1000
              setOnAt(end)
            }}
          />
        </Row>
      </div>

      <button
        onClick={handleRemove}
        className="mt-8 w-full text-xs text-mute/70 hover:text-red-400 transition py-2"
      >
        Remove this unit
      </button>
    </div>
  )
}

function Row({ label, children }: { label: string; children: React.ReactNode }) {
  return (
    <div className="flex items-center justify-between py-1.5 gap-3">
      <div className="text-sm text-mute">{label}</div>
      <div className="ml-auto">{children}</div>
    </div>
  )
}
function Divider() { return <div className="h-px bg-line my-1" /> }

function Toggle({ on, onChange }: { on: boolean; onChange: (v: boolean) => void }) {
  return (
    <button onClick={() => onChange(!on)} className={`w-12 h-7 rounded-full p-0.5 transition ${on ? 'bg-accent' : 'bg-line'}`}>
      <div className={`w-6 h-6 rounded-full bg-white transition ${on ? 'translate-x-5' : ''}`} />
    </button>
  )
}

function Pills<T extends number | string>({ options, value, onChange }:
  { options: [T, string][]; value: T; onChange: (v: T) => void }) {
  return (
    <div className="flex gap-1 flex-wrap justify-end">
      {options.map(([v, label]) => (
        <button key={String(v)} onClick={() => onChange(v)}
          className={`text-xs px-3 py-1.5 rounded-full transition ${
            value === v ? 'bg-accent text-ink font-medium' : 'bg-line/60 text-mute hover:text-white'
          }`}>
          {label}
        </button>
      ))}
    </div>
  )
}

function TimerPills({ timerEnd, onSet }: { timerEnd: number | null; onSet: (hours: number) => void }) {
  const choices: number[] = [0, 1, 4, 8]
  const remainingMs = timerEnd ? Math.max(0, timerEnd - Date.now()) : 0
  const totalSec = Math.ceil(remainingMs / 1000)
  const h = Math.floor(totalSec / 3600)
  const m = Math.floor((totalSec % 3600) / 60)
  const s = totalSec % 60
  const countdown = h > 0 ? `${h}:${String(m).padStart(2,'0')}:${String(s).padStart(2,'0')}`
                          : `${m}:${String(s).padStart(2,'0')}`
  return (
    <div className="flex items-center gap-2 justify-end">
      {timerEnd && <span className="text-[11px] text-accent tabular-nums">{countdown}</span>}
      <div className="flex gap-1">
        {choices.map(c => {
          const active = c === 0 ? timerEnd === null
                                 : timerEnd !== null && Math.abs((Date.now() + c * 3600 * 1000) - timerEnd) < 60_000
          return (
            <button key={c} onClick={() => onSet(c)}
              className={`text-xs px-2.5 py-1.5 rounded-full transition ${
                active ? 'bg-accent text-ink font-medium' : 'bg-line/60 text-mute hover:text-white'
              }`}>{c === 0 ? 'Off' : `${c}h`}</button>
          )
        })}
      </div>
    </div>
  )
}

