import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

// https://vitejs.dev/config/
export default defineConfig({
  base: "/admin-b64be512-4b03-4028-a589-13931942e205/",
  server: {
    host: "127.0.0.1",    
    proxy: {
      "^((?!(\/admin-b64be512-4b03-4028-a589-13931942e205)).)*$": "http://localhost:11000",
    },
  },
});
