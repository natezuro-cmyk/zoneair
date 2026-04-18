import type { CapacitorConfig } from '@capacitor/cli';

const config: CapacitorConfig = {
  appId: 'com.z1air.app',
  appName: 'Z1 Air',
  webDir: 'dist',
  server: {
    iosScheme: 'capacitor',
  },
  plugins: {
    SplashScreen: {
      launchAutoHide: true,
      launchShowDuration: 0,
      showSpinner: false,
    },
  },
};

export default config;
