import { useEffect, useRef, useState } from 'react'
import type { Unit } from '../state/units'
import { useUnits } from '../state/units'
import { UnitClient } from '../lib/unitClient'
import type { AcState, Mode, Fan, VSwing } from '../lib/unitClient'
import { analyzeState } from '../lib/alerts'

type Props = { unit: Unit; onBack: () => void; onDetails: () => void }

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


export default function UnitControl({ unit, onBack, onDetails }: Props) {
  const removeUnit = useUnits(s => s.remove)
  const renameUnit = useUnits(s => s.rename)
  const [client] = useState(() => new UnitClient(unit.host))
  const [showSettings, setShowSettings] = useState(false)
  const [resetting, setResetting] = useState(false)

  const handleRemove = () => {
    if (confirm(`Remove "${unit.name}" from the app?`)) {
      removeUnit(unit.id)
      onBack()
    }
  }

  const handleRename = () => {
    const name = prompt('Enter a new name for this unit:', unit.name)
    if (name && name.trim()) renameUnit(unit.id, name.trim())
  }

  const handleResetWifi = async () => {
    if (!confirm('Reset this unit\'s WiFi? It will disconnect and go back into setup mode (Z1Air-Setup-XXXX). You\'ll need to set it up again.')) return
    setResetting(true)
    try {
      await fetch(`http://${unit.host}/factory_reset`, { method: 'POST' })
    } catch {}
    removeUnit(unit.id)
    onBack()
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
    client.getState().then(s => { if (alive) setState(s) }).catch(() => {})
    try { ws = client.openSocket(s => { if (alive) setState(s) }) } catch {}
    return () => { alive = false; ws?.close() }
  }, [client])

  const view = { ...(state || {}), ...pending } as AcState
  const alertCount = state ? analyzeState(state).length : 0

  const send = (patch: Partial<AcState>) => {
    setPending(p => ({ ...p, ...patch }))
    client.sendCommand(patch as any).catch(() => {})
  }

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

  const [targetF, setTargetF] = useState<number | null>(null)
  const sendTimerRef = useRef<number | null>(null)
  useEffect(() => {
    if (state && targetF !== null && cToF(state.setpoint_c) === targetF) setTargetF(null)
  }, [state?.setpoint_c, targetF])

  const [targetPower, setTargetPower] = useState<boolean | null>(null)
  useEffect(() => {
    if (state && targetPower !== null && state.power === targetPower) setTargetPower(null)
  }, [state?.power, targetPower])

  if (!state) return (
    <div className="flex flex-col items-center justify-center h-full">
      <div className="spinner" />
      <div className="text-mute text-sm mt-4">Connecting to {unit.name}...</div>
      <button onClick={onBack} className="mt-6 text-sm text-dim">Back</button>
    </div>
  )

  const setF = targetF ?? cToF(view.setpoint_c)
  const indoorF = cToF(view.indoor_c)
  const clampF = (f: number) => Math.min(88, Math.max(63, f))

  const stepF = (delta: number) => {
    const next = clampF(setF + delta)
    setTargetF(next)
    if (sendTimerRef.current) window.clearTimeout(sendTimerRef.current)
    sendTimerRef.current = window.setTimeout(() => {
      client.sendCommand({ setpoint_f: next } as any).catch(() => {})
      sendTimerRef.current = null
    }, 250)
  }

  const powerOn = targetPower ?? view.power
  const modeLabel = ['Off','Cool','Heat','Auto','Dry','Fan'][view.mode] ?? '—'

  return (
    <div className="flex flex-col h-full fade-up">
      {/* Top bar */}
      <div className="flex items-center justify-between mb-2">
        <button onClick={onBack} className="text-dim text-sm font-light tracking-wide">← Back</button>
        <div className="text-sm font-light text-mute tracking-wide">{unit.name}</div>
        <button
          onClick={() => setShowSettings(!showSettings)}
          className="w-8 h-8 flex items-center justify-center rounded-full transition-all active:scale-90 relative"
          style={{ background: showSettings ? 'var(--bg-pill-active)' : 'var(--bg-pill)' }}
        >
          <svg width="18" height="18" viewBox="0 0 24 24" fill="currentColor" className={showSettings ? 'text-accent' : 'text-dim'}>
            <circle cx="12" cy="5" r="2" />
            <circle cx="12" cy="12" r="2" />
            <circle cx="12" cy="19" r="2" />
          </svg>
          {alertCount > 0 && (
            <div className="absolute -top-0.5 -right-0.5 w-3 h-3 rounded-full bg-red-500" />
          )}
        </button>
      </div>

      {/* Settings panel */}
      {showSettings && (
        <div className="glass rounded-2xl p-3 mb-3 space-y-1 fade-up">
          <button
            onClick={() => { setShowSettings(false); onDetails() }}
            className="w-full text-left px-3 py-2.5 rounded-xl text-sm font-light text-accent active:bg-white/5 transition flex items-center justify-between"
          >
            <span>Diagnostics</span>
            {alertCount > 0 && (
              <span className="px-2 py-0.5 rounded-full text-xs font-medium bg-red-500/20 text-red-400">{alertCount}</span>
            )}
          </button>
          <div className="h-px" style={{ background: 'var(--border)' }} />
          <button
            onClick={handleRename}
            className="w-full text-left px-3 py-2.5 rounded-xl text-sm font-light text-mute active:bg-white/5 transition"
          >
            Rename unit
          </button>
          <div className="h-px" style={{ background: 'var(--border)' }} />
          <button
            onClick={handleResetWifi}
            disabled={resetting}
            className="w-full text-left px-3 py-2.5 rounded-xl text-sm font-light text-orange-400 active:bg-white/5 transition disabled:opacity-50"
          >
            {resetting ? 'Resetting...' : 'Reset WiFi & remove unit'}
          </button>
          <div className="h-px" style={{ background: 'var(--border)' }} />
          <button
            onClick={handleRemove}
            className="w-full text-left px-3 py-2.5 rounded-xl text-sm font-light text-red-400 active:bg-white/5 transition"
          >
            Remove from app
          </button>
        </div>
      )}

      {/* Temperature display */}
      <div className="flex-shrink-0 flex items-center justify-center fade-up fade-up-delay-1 py-6">
        <div className="flex flex-col items-center">
          <div className="flex items-center gap-5">
            <button
              onClick={() => stepF(-1)}
              className="w-11 h-11 rounded-full flex items-center justify-center text-2xl text-dim active:text-white active:scale-90 transition-all"
              style={{ background: 'var(--bg-pill)' }}
            >
              −
            </button>
            <div className="tabular" style={{ fontSize: '72px', fontWeight: 200, lineHeight: 1, letterSpacing: '-2px' }}>
              {powerOn ? setF : '—'}
              <span style={{ fontSize: '32px', fontWeight: 300, opacity: powerOn ? 0.4 : 0.2 }}>°</span>
            </div>
            <button
              onClick={() => stepF(+1)}
              className="w-11 h-11 rounded-full flex items-center justify-center text-2xl text-dim active:text-white active:scale-90 transition-all"
              style={{ background: 'var(--bg-pill)' }}
            >
              +
            </button>
          </div>
          <div className="flex items-center gap-3 mt-1">
            <span className="text-xs font-light text-dim tracking-widest uppercase">{indoorF}° inside</span>
            {powerOn && <span className="text-xs text-accent/70 font-light">· {modeLabel}</span>}
          </div>
        </div>
      </div>

      {/* Controls panel */}
      <div className="flex-1 overflow-hidden no-scrollbar fade-up fade-up-delay-2 -mx-1 px-1">
        <div className="glass rounded-3xl p-4 space-y-0.5">
          <ControlRow label="Power">
            <Toggle
              on={powerOn}
              onChange={(v) => { setTargetPower(v); client.sendCommand({ power: v }).catch(() => {}) }}
            />
          </ControlRow>

          <Sep />

          <ControlRow label="Mode">
            <PillGroup options={MODES} value={view.mode} onChange={(v) => send({ mode: v })} />
          </ControlRow>

          <Sep />

          <ControlRow label="Fan">
            <PillGroup
              options={FAN_CHOICES}
              value={currentFanChoice(view)}
              onChange={(v) => {
                if (v === 'turbo')      send({ turbo: true,  mute: false })
                else if (v === 'quiet') send({ mute: true,   turbo: false })
                else                    send({ fan: FAN_TO_NUMERIC[v], turbo: false, mute: false })
              }}
            />
          </ControlRow>

          <Sep />

          <ControlRow label="Swing">
            <PillGroup options={SWINGS} value={view.vswing_pos} onChange={(v) => send({ vswing_pos: v })} />
          </ControlRow>

          <Sep />

          <ControlRow label="Eco">
            <Toggle on={view.eco} onChange={(v) => send({ eco: v })} />
          </ControlRow>
          <ControlRow label="Display">
            <Toggle on={view.display} onChange={(v) => send({ display: v })} />
          </ControlRow>
          <ControlRow label="Beep">
            <Toggle on={view.beep} onChange={(v) => send({ beep: v })} />
          </ControlRow>

          <Sep />

          <ControlRow label="Off in">
            <TimerPills
              timerEnd={offAt}
              onSet={(h) => setOffAt(h === 0 ? null : Date.now() + h * 3600 * 1000)}
            />
          </ControlRow>
          <ControlRow label="On in">
            <TimerPills
              timerEnd={onAt}
              onSet={(h) => setOnAt(h === 0 ? null : Date.now() + h * 3600 * 1000)}
            />
          </ControlRow>
        </div>

      </div>
    </div>
  )
}

/* ── Sub-components ── */

function ControlRow({ label, children }: { label: string; children: React.ReactNode }) {
  return (
    <div className="flex items-center justify-between py-2 gap-3">
      <div className="text-sm font-light text-mute tracking-wide">{label}</div>
      <div className="ml-auto">{children}</div>
    </div>
  )
}

function Sep() {
  return <div className="h-px" style={{ background: 'var(--border)' }} />
}

function Toggle({ on, onChange }: { on: boolean; onChange: (v: boolean) => void }) {
  return (
    <button onClick={() => onChange(!on)} className={`toggle-track ${on ? 'toggle-track-on' : ''}`}>
      <div className={`toggle-thumb ${on ? 'toggle-thumb-on' : ''}`} />
    </button>
  )
}

function PillGroup<T extends number | string>({ options, value, onChange }:
  { options: [T, string][]; value: T; onChange: (v: T) => void }) {
  return (
    <div className="flex gap-1.5 flex-wrap justify-end">
      {options.map(([v, label]) => (
        <button
          key={String(v)}
          onClick={() => onChange(v)}
          className={`pill ${value === v ? 'pill-active' : ''}`}
        >
          {label}
        </button>
      ))}
    </div>
  )
}

function TimerPills({ timerEnd, onSet }: { timerEnd: number | null; onSet: (hours: number) => void }) {
  const choices = [0, 1, 4, 8]
  const remainingMs = timerEnd ? Math.max(0, timerEnd - Date.now()) : 0
  const totalSec = Math.ceil(remainingMs / 1000)
  const h = Math.floor(totalSec / 3600)
  const m = Math.floor((totalSec % 3600) / 60)
  const s = totalSec % 60
  const countdown = h > 0 ? `${h}:${String(m).padStart(2,'0')}:${String(s).padStart(2,'0')}`
                          : `${m}:${String(s).padStart(2,'0')}`
  return (
    <div className="flex items-center gap-2 justify-end">
      {timerEnd && <span className="text-xs text-accent tabular font-light">{countdown}</span>}
      <div className="flex gap-1.5">
        {choices.map(c => {
          const active = c === 0 ? timerEnd === null
                                 : timerEnd !== null && Math.abs((Date.now() + c * 3600 * 1000) - timerEnd) < 60_000
          return (
            <button key={c} onClick={() => onSet(c)}
              className={`pill ${active ? 'pill-active' : ''}`}
            >{c === 0 ? 'Off' : `${c}h`}</button>
          )
        })}
      </div>
    </div>
  )
}
