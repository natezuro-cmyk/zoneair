import { create } from 'zustand'

export type Unit = {
  id: string        // local ID
  name: string      // user-given name, e.g. "Living Room"
  host: string      // ip or hostname, e.g. 192.168.1.44 or zoneair-livingroom.local
}

type UnitsState = {
  units: Unit[]
  add: (u: Unit) => void
  remove: (id: string) => void
  rename: (id: string, name: string) => void
}

const STORAGE_KEY = 'zoneair.units'

const load = (): Unit[] => {
  try {
    const raw = localStorage.getItem(STORAGE_KEY)
    return raw ? JSON.parse(raw) : []
  } catch { return [] }
}

const save = (u: Unit[]) => {
  try { localStorage.setItem(STORAGE_KEY, JSON.stringify(u)) } catch {}
}

export const useUnits = create<UnitsState>((set, get) => ({
  units: load(),
  add: (u) => { const next = [...get().units, u]; save(next); set({ units: next }) },
  remove: (id) => { const next = get().units.filter(x => x.id !== id); save(next); set({ units: next }) },
  rename: (id, name) => {
    const next = get().units.map(x => x.id === id ? { ...x, name } : x)
    save(next); set({ units: next })
  },
}))
