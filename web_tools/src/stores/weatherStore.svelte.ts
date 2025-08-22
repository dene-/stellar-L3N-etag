import { logStore } from "./logStore.svelte";

// Weather API response types
interface OpenMeteoResponse {
  latitude: number;
  longitude: number;
  generationtime_ms: number;
  utc_offset_seconds: number;
  timezone: string;
  timezone_abbreviation: string;
  elevation: number;
  daily_units: {
    time: string;
    weather_code: string;
    temperature_2m_max: string;
    temperature_2m_min: string;
    precipitation_probability_max: string;
    wind_direction_10m_dominant: string;
  };
  daily: {
    time: string[];
    weather_code: number[];
    temperature_2m_max: number[];
    temperature_2m_min: number[];
    precipitation_probability_max: number[];
    wind_direction_10m_dominant: number[];
  };
}

// Daily forecast data structure
export interface DailyForecast {
  date: string;
  weatherCode: number;
  temperatureMax: number;
  temperatureMin: number;
  precipitationProbability: number;
  windDirection: number;
}

// Complete weather data structure
export interface WeatherData {
  location: {
    latitude: number;
    longitude: number;
    timezone: string;
  };
  forecast: DailyForecast[];
}

interface Location {
  lat: string;
  lon: string;
  name: string;
}

type LocationResponse = Location[];

export class WeatherStore {
  weatherData = $state<WeatherData | null>(null);

  private getApiURL(lat: string, lon: string): string {
    return `https://api.open-meteo.com/v1/forecast?latitude=${lat}&longitude=${lon}&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max,wind_direction_10m_dominant&timezone=auto&forecast_days=8`;
  }

  private async getWeatherData(lat: string, lon: string): Promise<WeatherData> {
    const url = this.getApiURL(lat, lon);

    const response = await fetch(url);

    if (!response.ok) {
      throw new Error('Failed to fetch weather data');
    }

    const data = await response.json() as OpenMeteoResponse;

    const forecast: DailyForecast[] = data.daily.time.map((date, index) => ({
      date,
      weatherCode: data.daily.weather_code[index],
      temperatureMax: data.daily.temperature_2m_max[index],
      temperatureMin: data.daily.temperature_2m_min[index],
      precipitationProbability: data.daily.precipitation_probability_max[index],
      windDirection: data.daily.wind_direction_10m_dominant[index]
    }));

    return {
      location: {
        latitude: data.latitude,
        longitude: data.longitude,
        timezone: data.timezone
      },
      forecast
    };
  }

  private async getLocationLatLon(location: string): Promise<Location | null> {
    const response = await fetch(
      `https://nominatim.openstreetmap.org/search?format=json&q=${encodeURIComponent(
        location
      )}`
    );

    const geoData = await response.json() as LocationResponse;

    return geoData[0] || null;
  }

  // Public method to get weather forecast
  public async getForecast(location: string): Promise<void> {
    const locationData = await this.getLocationLatLon(location);

    if (!locationData) {
      logStore.addLog(`Location not found: ${location}`);
      return;
    }

    const { lat, lon } = locationData;

    this.weatherData = await this.getWeatherData(lat, lon);
  }

  // Helper method to get weather description from WMO code
  public getWeatherDescription(code: number): string {
    const weatherCodes: { [key: number]: string } = {
      0: 'Clear sky',
      1: 'Mainly clear',
      2: 'Partly cloudy',
      3: 'Overcast',
      45: 'Fog',
      48: 'Depositing rime fog',
      51: 'Light drizzle',
      53: 'Moderate drizzle',
      55: 'Dense drizzle',
      56: 'Light freezing drizzle',
      57: 'Dense freezing drizzle',
      61: 'Slight rain',
      63: 'Moderate rain',
      65: 'Heavy rain',
      66: 'Light freezing rain',
      67: 'Heavy freezing rain',
      71: 'Slight snow fall',
      73: 'Moderate snow fall',
      75: 'Heavy snow fall',
      77: 'Snow grains',
      80: 'Slight rain showers',
      81: 'Moderate rain showers',
      82: 'Violent rain showers',
      85: 'Slight snow showers',
      86: 'Heavy snow showers',
      95: 'Thunderstorm',
      96: 'Thunderstorm with slight hail',
      99: 'Thunderstorm with heavy hail'
    };
    return weatherCodes[code] || 'Unknown';
  }

  // Helper method to get wind direction from degrees
  public getWindDirection(degrees: number): string {
    const directions = ['N', 'NNE', 'NE', 'ENE', 'E', 'ESE', 'SE', 'SSE', 'S', 'SSW', 'SW', 'WSW', 'W', 'WNW', 'NW', 'NNW'];
    const index = Math.round(degrees / 22.5) % 16;
    return directions[index];
  }

  // Helper method to get weather icon category from WMO code
  public getIconCategory(code: number): string {
    if (code === 0) return 'sun';
    if ([1, 2].includes(code)) return 'sun-cloud';
    if (code === 3) return 'cloud';
    if ([45, 48].includes(code)) return 'fog';
    if (code >= 51 && code <= 57) return 'drizzle';
    if ((code >= 61 && code <= 67) || [80, 81, 82].includes(code)) return 'rain';
    if ((code >= 71 && code <= 77) || [85, 86].includes(code)) return 'snow';
    if ([95].includes(code)) return 'thunder';
    if ([96, 99].includes(code)) return 'thunder-hail';
    return 'cloud';
  }

  // Helper method to get weather icon SVG from WMO code
  public getWeatherIconSvg(code: number): string {
    const baseProps = 'stroke="black" stroke-width="3" fill="none" stroke-linecap="round" stroke-linejoin="round"';
    const small = 'stroke-width="2"';

    const icons: { [key: string]: string } = {
      sun: `<svg viewBox="0 0 64 64">
 <circle cx="32" cy="32" r="12" ${baseProps}/>
 <g ${baseProps}>
 <line x1="32" y1="6" x2="32" y2="14"/>
 <line x1="32" y1="50" x2="32" y2="58"/>
 <line x1="6" y1="32" x2="14" y2="32"/>
 <line x1="50" y1="32" x2="58" y2="32"/>
 <line x1="13" y1="13" x2="19" y2="19"/>
 <line x1="45" y1="45" x2="51" y2="51"/>
 <line x1="13" y1="51" x2="19" y2="45"/>
 <line x1="45" y1="19" x2="51" y2="13"/>
 </g>
</svg>`,
      cloud: `<svg viewBox="0 0 64 64">
 <path d="M20 46h26a10 10 0 0 0 0-20 16 16 0 0 0-31-2A10 10 0 0 0 20 46Z" ${baseProps}/>
</svg>`,
      'sun-cloud': `<svg viewBox="0 0 64 64">
 <circle cx="22" cy="22" r="9" ${baseProps}/>
 <path d="M20 50h22a9 9 0 0 0 0-18 14 14 0 0 0-27-2 9 9 0 0 0 5 20Z" ${baseProps}/>
</svg>`,
      fog: `<svg viewBox="0 0 64 64">
 <path d="M18 40h26a10 10 0 0 0 0-20 16 16 0 0 0-31-2A10 10 0 0 0 18 40Z" ${baseProps}/>
 <line x1="12" y1="46" x2="52" y2="46" ${baseProps}/>
 <line x1="16" y1="52" x2="48" y2="52" ${baseProps}/>
</svg>`,
      drizzle: `<svg viewBox="0 0 64 64">
 <path d="M20 40h22a9 9 0 0 0 0-18 14 14 0 0 0-27-2A9 9 0 0 0 20 40Z" ${baseProps}/>
 <g ${baseProps}>
 <line x1="22" y1="46" x2="20" y2="52"/>
 <line x1="32" y1="46" x2="30" y2="52"/>
 <line x1="42" y1="46" x2="40" y2="52"/>
 </g>
</svg>`,
      rain: `<svg viewBox="0 0 64 64">
 <path d="M20 38h24a10 10 0 0 0 0-20 16 16 0 0 0-31-2A10 10 0 0 0 20 38Z" ${baseProps}/>
 <g ${baseProps}>
 <line x1="22" y1="44" x2="18" y2="56"/>
 <line x1="32" y1="44" x2="28" y2="56"/>
 <line x1="42" y1="44" x2="38" y2="56"/>
 </g>
</svg>`,
      snow: `<svg viewBox="0 0 64 64">
 <path d="M20 38h24a10 10 0 0 0 0-20 16 16 0 0 0-31-2A10 10 0 0 0 20 38Z" ${baseProps}/>
 <g ${small} stroke="black" fill="none" stroke-linecap="round">
 <path d="M24 44l2 8"/>
 <path d="M24 44l-2 8"/>
 <path d="M32 44l2 8"/>
 <path d="M32 44l-2 8"/>
 <path d="M40 44l2 8"/>
 <path d="M40 44l-2 8"/>
 </g>
</svg>`,
      thunder: `<svg viewBox="0 0 64 64">
 <path d="M20 38h24a10 10 0 0 0 0-20 16 16 0 0 0-31-2A10 10 0 0 0 20 38Z" ${baseProps}/>
 <polyline points="30 40 24 54 34 50 30 62" ${baseProps} />
</svg>`,
      'thunder-hail': `<svg viewBox="0 0 64 64">
 <path d="M20 36h24a10 10 0 0 0 0-20 16 16 0 0 0-31-2A10 10 0 0 0 20 36Z" ${baseProps}/>
 <polyline points="30 38 24 52 34 48 30 60" ${baseProps} />
 <g ${baseProps}>
 <circle cx="22" cy="46" r="2"/>
 <circle cx="40" cy="46" r="2"/>
 </g>
</svg>`
    };

    const category = this.getIconCategory(code);
    return icons[category] || icons.cloud;
  }
}

// Create and export a singleton instance
export const weatherStore = new WeatherStore();