import { useState } from 'react'
import { useUnits } from '../state/units'
import { UnitClient } from '../lib/unitClient'

type Props = { onDone: () => void; onCancel: () => void }

export default function AddUnit({ onDone, onCancel }: Props) {
  const add = useUnits(s => s.add)
  const [name, setName] = useState('Living Room')
  const [host, setHost] = useState('192.168.1.44')
  const [status, setStatus] = useState<string | null>(null)
  const [busy, setBusy] = useState(false)

  const tryAdd = async () => {
    setBusy(true); setStatus('Checking…')
    try {
      await new UnitClient(host).getState()
      add({ id: crypto.randomUUID(), name: name.trim() || host, host: host.trim() })
      setStatus('Added.')
      setTimeout(onDone, 400)
    } catch (e: any) {
      setStatus(`Could not reach ${host}. Check the IP and that the unit is online.`)
    } finally {
      setBusy(false)
    }
  }

  return (
    <div>
      <button onClick={onCancel} className="text-mute text-sm mb-6">← Cancel</button>
      <div className="text-lg font-medium mb-1">Add a unit</div>
      <p className="text-mute text-sm mb-6">During development we add by IP. Provisioning will use Bluetooth in v1.</p>

      <Field label="Name">
        <input
          value={name}
          onChange={e => setName(e.target.value)}
          className="bg-line/40 rounded-xl px-4 py-3 w-full text-base"
        />
      </Field>
      <Field label="IP or hostname">
        <input
          value={host}
          onChange={e => setHost(e.target.value)}
          className="bg-line/40 rounded-xl px-4 py-3 w-full text-base font-mono"
          placeholder="192.168.1.44 or z1air-livingroom.local"
        />
      </Field>

      <button
        onClick={tryAdd}
        disabled={busy}
        className="mt-6 w-full bg-accent text-ink font-medium py-3 rounded-full disabled:opacity-50"
      >
        {busy ? 'Adding…' : 'Add unit'}
      </button>

      {status && <div className="mt-4 text-sm text-mute">{status}</div>}
    </div>
  )
}

function Field({ label, children }: { label: string; children: React.ReactNode }) {
  return (
    <label className="block mb-4">
      <div className="text-xs uppercase tracking-widest text-mute mb-2">{label}</div>
      {children}
    </label>
  )
}
