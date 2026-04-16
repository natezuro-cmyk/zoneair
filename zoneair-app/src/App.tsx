import { useState } from 'react'
import { useUnits } from './state/units'
import UnitList from './pages/UnitList'
import UnitControl from './pages/UnitControl'
import AddUnit from './pages/AddUnit'

type Page = { kind: 'list' } | { kind: 'control'; unitId: string } | { kind: 'add' }

export default function App() {
  const [page, setPage] = useState<Page>({ kind: 'list' })
  const units = useUnits(s => s.units)

  return (
    <div className="min-h-full max-w-md mx-auto px-5 pt-12 pb-16">
      <Header />
      {page.kind === 'list' && (
        <UnitList
          onPick={(id) => setPage({ kind: 'control', unitId: id })}
          onAdd={() => setPage({ kind: 'add' })}
        />
      )}
      {page.kind === 'control' && (
        <UnitControl
          unit={units.find(u => u.id === page.unitId)!}
          onBack={() => setPage({ kind: 'list' })}
        />
      )}
      {page.kind === 'add' && (
        <AddUnit
          onDone={() => setPage({ kind: 'list' })}
          onCancel={() => setPage({ kind: 'list' })}
        />
      )}
    </div>
  )
}

function Header() {
  return (
    <div className="flex items-center justify-between mb-8">
      <div>
        <div className="text-xs uppercase tracking-[0.2em] text-mute">Z1 Air</div>
        <div className="text-xl font-medium mt-1">Climate</div>
      </div>
    </div>
  )
}
