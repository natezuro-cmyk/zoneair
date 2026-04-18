import { describe, it, expect, beforeEach, vi } from 'vitest'
import { act } from '@testing-library/react'

// The Zustand store reads localStorage at module import time, so we use
// vi.resetModules() + dynamic imports to get a fresh store for each test.

describe('useUnits store', () => {
  beforeEach(() => {
    vi.resetModules()
    localStorage.clear()
  })

  it('starts with an empty units list when localStorage is empty', async () => {
    const { useUnits } = await import('./units')
    expect(useUnits.getState().units).toEqual([])
  })

  it('add() appends a unit', async () => {
    const { useUnits } = await import('./units')
    const unit = { id: '1', name: 'Living Room', host: '192.168.1.10' }

    act(() => useUnits.getState().add(unit))

    expect(useUnits.getState().units).toHaveLength(1)
    expect(useUnits.getState().units[0]).toEqual(unit)
  })

  it('add() can add multiple units', async () => {
    const { useUnits } = await import('./units')
    act(() => {
      useUnits.getState().add({ id: '1', name: 'Room A', host: '192.168.1.10' })
      useUnits.getState().add({ id: '2', name: 'Room B', host: '192.168.1.11' })
    })
    expect(useUnits.getState().units).toHaveLength(2)
  })

  it('remove() deletes the matching unit by id', async () => {
    const { useUnits } = await import('./units')
    const unit = { id: '1', name: 'Bedroom', host: '192.168.1.10' }

    act(() => useUnits.getState().add(unit))
    act(() => useUnits.getState().remove('1'))

    expect(useUnits.getState().units).toHaveLength(0)
  })

  it('remove() leaves other units untouched', async () => {
    const { useUnits } = await import('./units')
    act(() => {
      useUnits.getState().add({ id: '1', name: 'Room A', host: '192.168.1.10' })
      useUnits.getState().add({ id: '2', name: 'Room B', host: '192.168.1.11' })
    })
    act(() => useUnits.getState().remove('1'))

    const units = useUnits.getState().units
    expect(units).toHaveLength(1)
    expect(units[0].id).toBe('2')
  })

  it('remove() is a no-op for an id that does not exist', async () => {
    const { useUnits } = await import('./units')
    const unit = { id: '1', name: 'Room A', host: '192.168.1.10' }
    act(() => useUnits.getState().add(unit))
    act(() => useUnits.getState().remove('999'))

    expect(useUnits.getState().units).toHaveLength(1)
  })

  it('rename() updates the name of the matching unit', async () => {
    const { useUnits } = await import('./units')
    act(() => useUnits.getState().add({ id: '1', name: 'Old Name', host: '192.168.1.10' }))
    act(() => useUnits.getState().rename('1', 'New Name'))

    expect(useUnits.getState().units[0].name).toBe('New Name')
  })

  it('rename() does not change other units', async () => {
    const { useUnits } = await import('./units')
    act(() => {
      useUnits.getState().add({ id: '1', name: 'Room A', host: '192.168.1.10' })
      useUnits.getState().add({ id: '2', name: 'Room B', host: '192.168.1.11' })
    })
    act(() => useUnits.getState().rename('1', 'Bedroom'))

    const units = useUnits.getState().units
    expect(units.find(u => u.id === '2')?.name).toBe('Room B')
  })

  it('rename() does not change host or id', async () => {
    const { useUnits } = await import('./units')
    act(() => useUnits.getState().add({ id: '1', name: 'Room A', host: '192.168.1.10' }))
    act(() => useUnits.getState().rename('1', 'Renamed'))

    const unit = useUnits.getState().units[0]
    expect(unit.id).toBe('1')
    expect(unit.host).toBe('192.168.1.10')
  })

  it('persists units to localStorage after add()', async () => {
    const { useUnits } = await import('./units')
    act(() => useUnits.getState().add({ id: '1', name: 'Living Room', host: '192.168.1.10' }))

    const stored = JSON.parse(localStorage.getItem('zoneair.units') ?? '[]')
    expect(stored).toHaveLength(1)
    expect(stored[0].id).toBe('1')
  })

  it('persists units to localStorage after remove()', async () => {
    const { useUnits } = await import('./units')
    act(() => {
      useUnits.getState().add({ id: '1', name: 'Room A', host: '192.168.1.10' })
      useUnits.getState().add({ id: '2', name: 'Room B', host: '192.168.1.11' })
    })
    act(() => useUnits.getState().remove('1'))

    const stored = JSON.parse(localStorage.getItem('zoneair.units') ?? '[]')
    expect(stored).toHaveLength(1)
    expect(stored[0].id).toBe('2')
  })

  it('loads pre-existing units from localStorage on init', async () => {
    const saved = [{ id: 'abc', name: 'Garage', host: '10.0.0.5' }]
    localStorage.setItem('zoneair.units', JSON.stringify(saved))

    const { useUnits } = await import('./units')

    expect(useUnits.getState().units).toEqual(saved)
  })

  it('starts with empty list if localStorage contains invalid JSON', async () => {
    localStorage.setItem('zoneair.units', 'not valid json {{')

    const { useUnits } = await import('./units')

    expect(useUnits.getState().units).toEqual([])
  })
})
