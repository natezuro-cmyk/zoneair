import { useState } from 'react'
import { useUnits } from './state/units'
import UnitList from './pages/UnitList'
import UnitControl from './pages/UnitControl'
import UnitDetails from './pages/UnitDetails'
import AddUnit from './pages/AddUnit'
import SetupWifi from './pages/SetupWifi'
import logo from './assets/logo.png'

type Tab = 'home' | 'diagnostics'

type Page =
  | { kind: 'list' }
  | { kind: 'control'; unitId: string }
  | { kind: 'diagPicker' }
  | { kind: 'details'; unitId: string }
  | { kind: 'add' }
  | { kind: 'setup' }

export default function App() {
  const units = useUnits(s => s.units)
  const [page, setPage] = useState<Page>(units.length === 0 ? { kind: 'setup' } : { kind: 'list' })
  const [activeTab, setActiveTab] = useState<Tab>('home')

  const showTabs = page.kind !== 'setup' && page.kind !== 'add'

  const goHome = () => {
    setActiveTab('home')
    if (page.kind === 'details' || page.kind === 'diagPicker') {
      setPage({ kind: 'list' })
    }
  }

  const goDiagnostics = () => {
    setActiveTab('diagnostics')
    // If viewing a specific unit, go straight to its diagnostics
    if (page.kind === 'control') {
      setPage({ kind: 'details', unitId: page.unitId })
    } else if (page.kind === 'details') {
      // already there
    } else if (units.length === 1) {
      // Only one unit, go straight to it
      setPage({ kind: 'details', unitId: units[0].id })
    } else {
      // Multiple units — show picker
      setPage({ kind: 'diagPicker' })
    }
  }

  return (
    <div className="h-full flex flex-col" style={{ paddingTop: 'calc(var(--safe-top) + 16px)' }}>
      <Header />
      <div className="flex-1 overflow-hidden px-5">
        {page.kind === 'setup' && (
          <SetupWifi onComplete={() => setPage({ kind: 'list' })} />
        )}
        {page.kind === 'list' && (
          <UnitList
            onPick={(id) => { setActiveTab('home'); setPage({ kind: 'control', unitId: id }) }}
            onAdd={() => setPage({ kind: 'add' })}
          />
        )}
        {page.kind === 'control' && (
          <UnitControl
            unit={units.find(u => u.id === page.unitId)!}
            onBack={() => { setActiveTab('home'); setPage({ kind: 'list' }) }}
            onDetails={() => { setActiveTab('diagnostics'); setPage({ kind: 'details', unitId: page.unitId }) }}
          />
        )}
        {page.kind === 'diagPicker' && (
          <DiagPicker
            units={units}
            onPick={(id) => setPage({ kind: 'details', unitId: id })}
          />
        )}
        {page.kind === 'details' && (
          <UnitDetails
            unit={units.find(u => u.id === page.unitId)!}
            onBack={() => {
              if (units.length > 1) {
                setPage({ kind: 'diagPicker' })
              } else {
                setActiveTab('home')
                setPage({ kind: 'list' })
              }
            }}
            onSwitchUnit={units.length > 1 ? () => setPage({ kind: 'diagPicker' }) : undefined}
          />
        )}
        {page.kind === 'add' && (
          <AddUnit
            onDone={() => setPage({ kind: 'list' })}
            onCancel={() => setPage({ kind: 'list' })}
          />
        )}
      </div>

      {/* Bottom tab bar */}
      {showTabs && (
        <div
          className="flex items-center justify-around px-8"
          style={{
            paddingBottom: 'calc(var(--safe-bottom) + 8px)',
            paddingTop: '8px',
            borderTop: '1px solid var(--border)',
          }}
        >
          <TabButton
            active={activeTab === 'home'}
            label="Home"
            onClick={goHome}
            icon={
              <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round">
                <path d="M3 10.5L12 3l9 7.5V21a1 1 0 01-1 1H4a1 1 0 01-1-1z" />
                <path d="M9 22V12h6v10" />
              </svg>
            }
          />
          <TabButton
            active={activeTab === 'diagnostics'}
            label="Diagnostics"
            onClick={goDiagnostics}
            icon={
              <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round">
                <path d="M22 12h-4l-3 9L9 3l-3 9H2" />
              </svg>
            }
          />
        </div>
      )}
    </div>
  )
}

function Header() {
  return (
    <div className="flex justify-center pt-4 pb-5 px-5">
      <img
        src={logo}
        alt="Z1 Air"
        className="h-10"
        style={{ filter: 'invert(1)' }}
      />
    </div>
  )
}

function TabButton({ active, label, onClick, icon }: {
  active: boolean; label: string; onClick: () => void
  icon: React.ReactNode
}) {
  return (
    <button
      onClick={onClick}
      className={`flex flex-col items-center gap-1 transition-all active:scale-95 ${active ? 'text-accent' : 'text-dim'}`}
    >
      {icon}
      <span className="text-[10px] font-light tracking-wide">{label}</span>
    </button>
  )
}

function DiagPicker({ units, onPick }: { units: { id: string; name: string }[]; onPick: (id: string) => void }) {
  return (
    <div className="fade-up">
      <div className="text-xl font-medium tracking-tight mb-1">Diagnostics</div>
      <p className="text-mute text-sm font-light mb-6">Select a unit to view.</p>
      <div className="space-y-3">
        {units.map((u, i) => (
          <button
            key={u.id}
            onClick={() => onPick(u.id)}
            className={`w-full text-left glass glass-hover rounded-2xl p-5 flex items-center justify-between transition-all active:scale-[0.98] fade-up fade-up-delay-${Math.min(i + 1, 4)}`}
          >
            <div className="text-base font-medium">{u.name}</div>
            <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5" className="text-dim">
              <path d="M9 18l6-6-6-6" />
            </svg>
          </button>
        ))}
      </div>
    </div>
  )
}
