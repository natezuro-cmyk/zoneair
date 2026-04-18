import { useEffect, useState } from 'react'
import { useUnits } from '../state/units'
import { UnitClient } from '../lib/unitClient'
import type { AcState } from '../lib/unitClient'

type Props = { onPick: (id: string) => void; onAdd: () => void }

const cToF = (c: number) => Math.round(c * 9/5 + 32)

export default function UnitList({ onPick, onAdd }: Props) {
  const units = useUnits(s => s.units)
  const [states, setStates] = useState<Record<string, AcState | null>>({})

  useEffect(() => {
    let cancelled = false
    const tick = async () => {
      for (const u of units) {
        try {
          const s = await new UnitClient(u.host).getState()
          if (!cancelled) setStates(prev => ({ ...prev, [u.id]: s }))
        } catch {
          if (!cancelled) setStates(prev => ({ ...prev, [u.id]: null }))
        }
      }
    }
    tick()
    const i = setInterval(tick, 5000)
    return () => { cancelled = true; clearInterval(i) }
  }, [units])

  if (units.length === 0) return <Empty onAdd={onAdd} />

  return (
    <div className="space-y-3 fade-up">
      {units.map((u, i) => {
        const s = states[u.id]
        return (
          <button
            key={u.id}
            onClick={() => onPick(u.id)}
            className={`w-full text-left glass glass-hover rounded-2xl p-5 flex items-center justify-between transition-all active:scale-[0.98] fade-up fade-up-delay-${Math.min(i + 1, 4)}`}
          >
            <div>
              <div className="text-base font-medium">{u.name}</div>
              <div className="text-xs text-dim mt-1 font-light">
                {s == null ? 'Offline' : s.power ? labelMode(s.mode) : 'Off'}
              </div>
            </div>
            <div className="text-right">
              {s == null ? (
                <div className="w-2 h-2 rounded-full bg-dim" />
              ) : (
                <>
                  <div className="tabular font-extralight" style={{ fontSize: '36px', lineHeight: 1, letterSpacing: '-1px' }}>
                    {cToF(s.indoor_c)}°
                  </div>
                  {s.power && (
                    <div className="text-xs text-accent/60 mt-1 font-light">
                      Set {cToF(s.setpoint_c)}°
                    </div>
                  )}
                </>
              )}
            </div>
          </button>
        )
      })}
      <button
        onClick={onAdd}
        className="w-full mt-2 rounded-2xl py-4 text-dim text-sm font-light tracking-wide transition-all active:scale-[0.98]"
        style={{ border: '1px dashed var(--border)' }}
      >
        + Add unit
      </button>
    </div>
  )
}

function Empty({ onAdd }: { onAdd: () => void }) {
  return (
    <div className="flex flex-col items-center justify-center h-full fade-up">
      <div className="text-dim text-sm font-light mb-6">No units yet</div>
      <button
        onClick={onAdd}
        className="px-8 py-3 rounded-full text-sm font-medium transition-all active:scale-95"
        style={{ background: 'var(--accent)', color: '#08090c' }}
      >
        Add your first unit
      </button>
    </div>
  )
}

function labelMode(m: number) {
  return ['Off','Cool','Heat','Auto','Dry','Fan'][m] ?? '—'
}
