import { useState } from 'react'
import { useUnits } from '../state/units'
import { UnitClient } from '../lib/unitClient'

type Props = { onComplete: () => void }

type Step = 'connect-ap' | 'finding' | 'name-unit'

const DEFAULT_HOST = 'z1air-unit.local'

export default function SetupWifi({ onComplete }: Props) {
  const [step, setStep] = useState<Step>('connect-ap')
  const [status, setStatus] = useState<string | null>(null)
  const [found, setFound] = useState(false)
  const [location, setLocation] = useState('')
  const add = useUnits(s => s.add)

  const scanForUnit = async () => {
    setStep('finding')
    setStatus('Looking for your unit on the network...')
    setFound(false)

    for (let attempt = 0; attempt < 6; attempt++) {
      try {
        await new UnitClient(DEFAULT_HOST).getState()
        setFound(true)
        setStatus(null)
        return
      } catch {
        if (attempt < 5) {
          setStatus(`Searching... (attempt ${attempt + 2}/6)`)
          await new Promise(r => setTimeout(r, 3000))
        }
      }
    }
    setStatus('Could not find the unit. Make sure you completed the WiFi setup and are back on your home WiFi.')
  }

  const addUnit = () => {
    if (!location.trim()) return
    add({ id: crypto.randomUUID(), name: location.trim(), host: DEFAULT_HOST })
    onComplete()
  }

  return (
    <div className="fade-up">
      <div className="text-xl font-medium tracking-tight mb-1">Set up your unit</div>
      <p className="text-mute text-sm font-light mb-8">Connect your Z1 Air to WiFi.</p>

      {step === 'connect-ap' && (
        <div className="fade-up fade-up-delay-1">
          <div className="glass rounded-3xl p-5 space-y-4 text-sm font-light">
            <p className="text-mute">
              Open your phone's <span className="text-white/90 font-normal">WiFi Settings</span> and connect to the network created by your AC unit:
            </p>
            <div
              className="rounded-2xl px-4 py-3 text-center text-base tracking-wider text-accent"
              style={{ background: 'rgba(62, 166, 255, 0.08)', border: '1px solid rgba(62, 166, 255, 0.15)' }}
            >
              Z1Air-Setup-XXXX
            </div>
            <p className="text-dim text-xs">The last 4 characters are unique to your unit.</p>
            <p className="text-mute">
              A setup page will appear automatically. Enter your <span className="text-white/90 font-normal">home WiFi name and password</span>, then tap connect.
            </p>
            <p className="text-mute">
              Once done, reconnect to your home network and return here.
            </p>
          </div>
          <button
            onClick={scanForUnit}
            className="mt-6 w-full py-3.5 rounded-full text-sm font-medium transition-all active:scale-[0.97]"
            style={{ background: 'var(--accent)', color: '#08090c' }}
          >
            I'm done — find my unit
          </button>
        </div>
      )}

      {step === 'finding' && (
        <div className="fade-up">
          {!found && (
            <div className="flex flex-col items-center pt-12">
              <div className="spinner" />
              {status && <div className="text-sm text-mute font-light text-center mt-6">{status}</div>}
              {status?.startsWith('Could not') && (
                <button
                  onClick={() => { setStep('connect-ap'); setStatus(null) }}
                  className="mt-6 text-sm text-dim"
                >
                  ← Start over
                </button>
              )}
            </div>
          )}

          {found && (
            <div className="fade-up">
              <div className="glass rounded-3xl p-6 text-center">
                <div className="text-3xl mb-2 text-accent">&#10003;</div>
                <div className="text-base font-medium">Unit found</div>
              </div>
              <button
                onClick={() => setStep('name-unit')}
                className="mt-6 w-full py-3.5 rounded-full text-sm font-medium transition-all active:scale-[0.97]"
                style={{ background: 'var(--accent)', color: '#08090c' }}
              >
                Next
              </button>
            </div>
          )}
        </div>
      )}

      {step === 'name-unit' && (
        <div className="fade-up">
          <div className="text-base font-medium mb-2">Where is this unit?</div>
          <p className="text-mute text-sm font-light mb-6">Give it a location name.</p>
          <label className="block mb-4">
            <div className="text-xs uppercase tracking-widest text-dim mb-2 font-light">Location</div>
            <input
              value={location}
              onChange={e => setLocation(e.target.value)}
              className="glass rounded-2xl px-4 py-3.5 w-full text-base font-light focus:outline-none focus:ring-1 focus:ring-accent/30"
              style={{ background: 'var(--bg-card)' }}
              placeholder="e.g. Living Room, Bedroom, Office"
              autoFocus
            />
          </label>
          <button
            onClick={addUnit}
            disabled={!location.trim()}
            className="mt-4 w-full py-3.5 rounded-full text-sm font-medium transition-all active:scale-[0.97] disabled:opacity-30"
            style={{ background: 'var(--accent)', color: '#08090c' }}
          >
            Add unit
          </button>
        </div>
      )}
    </div>
  )
}
