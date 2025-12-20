export const theme = {
  colors: {
    dark: '#000000',
    light: '#00FFFF',
    background: '#0a0a12',
    backgroundPanel: '#12121a',
    foreground: '#A8B5A8',
    accent: '#6B9E6B',
    accentHover: '#5A8A5A',
    accentActive: '#4A7A4A',
    border: '#3A4A3A',
    borderLight: '#4A5A4A',
    error: '#D97373',
    success: '#6B9E6B',
    warning: '#D9D973',
    info: '#7373D9',
    hover: {
      primary: '#5A8A5A',
      secondary: 'rgba(107, 158, 107, 0.08)'
    },
    pressed: {
      primary: '#3A4A3A',
      secondary: '#2A3A2A'
    },
    focus: {
      ring: '#6B9E6B',
      glow: '#7AAE7A'
    },
    disabled: {
      thumb: '#3A4A3A',
      border: '#4A5A4A',
      range: '#3A4A3A',
      track: '#2A3A2A',
      text: '#5A6A5A'
    },
    crt: {
      scanline: 'rgba(0, 0, 0, 0.3)',
      glow: 'rgba(0, 255, 128, 0.1)'
    }
  },

  font: {
    family: 'JetBrains Mono, Fira Code, Inconsolata, monospace',
    size: {
      xs: 'clamp(0.7rem, 0.8vw, 0.8rem)',
      sm: 'clamp(0.8rem, 1vw, 0.9rem)',
      md: 'clamp(0.9rem, 1.2vw, 1rem)',
      lg: 'clamp(1rem, 1.5vw, 1.13rem)',
      xl: 'clamp(1.13rem, 2vw, 1.25rem)',
      heading: 'clamp(1.25rem, 3vw, 1.5rem)'
    }
  },

  spacing: {
    xs: '0.29rem',
    sm: '0.57rem',
    md: '0.86rem',
    lg: '1.14rem',
    xl: '1.71rem',
    xxl: '2.57rem'
  },

  radius: {
    none: '0rem',
    sm: '0.14rem',
    md: '0.29rem',
    lg: '0.57rem'
  },

  shadow: {
    glow: '0 0 0.29rem rgba(107, 158, 107, 0.5)',
    inner: 'inset 0 0 0.14rem rgba(107, 158, 107, 0.3)',
    panel: '0 4px 20px rgba(0, 0, 0, 0.4)'
  },

  breakpoints: {
    xs: '320px',
    sm: '480px',
    md: '768px',
    lg: '1024px',
    xl: '1280px',
    xxl: '1536px'
  }
}
