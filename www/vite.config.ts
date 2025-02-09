import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

// https://vitejs.dev/config/
export default defineConfig({
  base: "/",
  server: {
    host: "127.0.0.1",    
    proxy: {
      "^((?!(\/admin-b64be512-4b03-4028-a589-13931942e205)).)*$": "http://localhost:11000",
    },
  },
  build: {
    rollupOptions: {
      output: {
        assetFileNames: (assetInfo) => { 
          return "admin-b64be5124b034028a58913931942e205/[name]-[hash].[extname]";
        },
        chunkFileNames: 'admin-b64be5124b034028a58913931942e205/js/[name]-[hash].js',
        entryFileNames: 'admin-b64be5124b034028a58913931942e205/js/[name]-[hash].js'
      }
    }
  }
});
