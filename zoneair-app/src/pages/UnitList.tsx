import { useEffect, useState } from 'react'
import { useUnits } from '../state/units'
import { UnitClient } from '../lib/unitClient'
import type { AcState } from '../lib/unitClient'

type Props = { onPick: (id: string) => void; onAdd: () => void }

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
    <div className="space-y-3">
      {units.map(u => {
        const s = states[u.id]
        return (
          <button
            key={u.id}
            onClick={() => onPick(u.id)}
            className="w-full text-left bg-line/40 hover:bg-line/70 transition rounded-2xl p-5 flex items-center justify-between"
          >
            <div>
              <div className="text-base font-medium">{u.name}</div>
              <div className="text-xs text-mute mt-1">{u.host}</div>
            </div>
            <div className="text-right">
              {s == null ? (
                <span className="text-mute text-xs">offline</span>
              ) : (
                <>
                  <div className="text-2xl font-light tabular-nums">{Math.round(s.indoor_c * 9/5 + 32)}°</div>
                  <div className="text-xs text-mute mt-0.5">{labelMode(s.mode)} · {Math.round(s.setpoint_c * 9/5 + 32)}°</div>
                </>
              )}
            </div>
          </button>
        )
      })}
      <button
        onClick={onAdd}
        className="w-full mt-4 border border-dashed border-line text-mute rounded-2xl py-4 hover:text-white hover:border-mute transition"
      >
        + Add unit
      </button>
    </div>
  )
}

function Empty({ onAdd }: { onAdd: () => void }) {
  return (
    <div className="mt-24 text-center">
      <div className="text-mute text-sm mb-6">No units yet.</div>
      <button onClick={onAdd} className="bg-accent text-ink font-medium px-6 py-3 rounded-full">
        Add your first unit
      </button>
    </div>
  )
}

function labelMode(m: number) {
  return ['Off','Cool','Heat','Auto','Dry','Fan'][m] ?? '—'
}
