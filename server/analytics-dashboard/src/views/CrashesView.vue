<template>
  <AppLayout>
    <div class="page">
      <div class="page-header">
        <h2>Crashes</h2>
        <DateRangePicker v-model="range" />
      </div>

      <div v-if="loading" class="loading">Loading...</div>
      <div v-else-if="error" class="error">{{ error }}</div>
      <template v-else-if="data">
        <div class="chart-section">
          <h3>Crash Rate by Version</h3>
          <BarChart :data="versionChartData" />
        </div>

        <div class="grid-2col">
          <div class="chart-section">
            <h3>Crashes by Signal</h3>
            <PieChart :data="signalChartData" />
          </div>
          <MetricCard
            title="Average Uptime"
            :value="formatDuration(data.avg_uptime_sec)"
            subtitle="before crash"
            color="var(--accent-yellow)"
          />
        </div>
      </template>
    </div>
  </AppLayout>
</template>

<script setup lang="ts">
import { ref, watch, computed } from 'vue'
import AppLayout from '@/components/AppLayout.vue'
import DateRangePicker from '@/components/DateRangePicker.vue'
import BarChart from '@/components/BarChart.vue'
import PieChart from '@/components/PieChart.vue'
import MetricCard from '@/components/MetricCard.vue'
import { api } from '@/services/api'
import type { CrashesData } from '@/services/api'

const COLORS = ['#3b82f6', '#10b981', '#f59e0b', '#ef4444', '#8b5cf6', '#ec4899', '#06b6d4', '#84cc16']

const range = ref('30d')
const data = ref<CrashesData | null>(null)
const loading = ref(true)
const error = ref('')

function formatDuration(seconds: number): string {
  const h = Math.floor(seconds / 3600)
  const m = Math.floor((seconds % 3600) / 60)
  return h > 0 ? `${h}h ${m}m` : `${m}m`
}

const versionChartData = computed(() => ({
  labels: data.value?.by_version.map(v => v.version) ?? [],
  datasets: [{
    label: 'Crash Rate %',
    data: data.value?.by_version.map(v => v.rate) ?? [],
    backgroundColor: '#ef4444'
  }]
}))

const signalChartData = computed(() => ({
  labels: data.value?.by_signal.map(s => s.signal) ?? [],
  datasets: [{
    data: data.value?.by_signal.map(s => s.count) ?? [],
    backgroundColor: COLORS
  }]
}))

async function fetchData() {
  loading.value = true
  error.value = ''
  try {
    data.value = await api.getCrashes(range.value)
  } catch (e) {
    error.value = e instanceof Error ? e.message : 'Failed to load data'
  } finally {
    loading.value = false
  }
}

watch(range, fetchData, { immediate: true })
</script>

<style scoped>
.page-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin-bottom: 24px;
}

.page-header h2 {
  font-size: 20px;
  font-weight: 600;
}

.grid-2col {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 16px;
  margin-bottom: 24px;
  align-items: start;
}

.chart-section {
  margin-bottom: 24px;
}

.chart-section h3 {
  font-size: 14px;
  font-weight: 500;
  color: var(--text-secondary);
  margin-bottom: 12px;
}

.loading, .error {
  padding: 40px;
  text-align: center;
  color: var(--text-secondary);
}

.error {
  color: var(--accent-red);
}
</style>
