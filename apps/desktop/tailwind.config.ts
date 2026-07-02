import type { Config } from "tailwindcss";

export default {
  content: ["./index.html", "./src/**/*.{ts,tsx}"],
  theme: {
    extend: {
      colors: {
        panel: "#16181d",
        surface: "#20232a",
        line: "#303640",
        accent: "#2dd4bf"
      }
    }
  },
  plugins: []
} satisfies Config;

