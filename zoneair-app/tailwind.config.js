/** @type {import('tailwindcss').Config} */
export default {
  content: ['./index.html', './src/**/*.{ts,tsx}'],
  theme: {
    extend: {
      colors: {
        ink: '#08090c',
        card: 'rgba(255, 255, 255, 0.04)',
        line: 'rgba(255, 255, 255, 0.06)',
        mute: 'rgba(237, 242, 247, 0.45)',
        dim: 'rgba(237, 242, 247, 0.25)',
        accent: '#3ea6ff',
      },
      fontFamily: {
        sans: ['Outfit', 'system-ui', 'sans-serif'],
      },
      borderRadius: {
        '2xl': '16px',
        '3xl': '20px',
      },
    },
  },
  plugins: [],
}
