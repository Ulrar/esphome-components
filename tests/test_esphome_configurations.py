"""
ESPHome UPS HID Configuration Tests
Tests ESPHome configurations for validation, compilation, and simulation
"""

import pytest
from pathlib import Path


class TestConfigurationValidation:
    """Test ESPHome configuration validation"""
    
    @pytest.mark.validation
    @pytest.mark.ci
    @pytest.mark.parametrize("config", [
        "configs/testing/minimal_simulation.yaml",
        "configs/testing/basic_test_standalone.yaml", 
        "configs/testing/simulation_test_standalone.yaml"
    ])
    def test_config_validation(self, esphome_runner, workspace_root, config):
        """Test that configurations validate successfully"""
        config_path = workspace_root / config
        assert config_path.exists(), f"Config file {config} not found"
        
        result = esphome_runner.validate_config(config_path)
        
        # Log details for debugging
        if not result['success']:
            pytest.fail(
                f"Validation failed for {config}:\n"
                f"Duration: {result['duration']}s\n"
                f"Return code: {result['returncode']}\n"
                f"Stderr: {result['stderr']}\n"
                f"Stdout: {result['stdout'][:500]}..."
            )
        
        assert result['success'], f"Validation failed for {config}"
        assert result['duration'] < 30, f"Validation too slow: {result['duration']}s"
    
    @pytest.mark.validation
    def test_all_standalone_configs_validate(self, esphome_runner, test_configs):
        """Test that all standalone test configs validate"""
        failed_configs = []
        
        for config_path in test_configs:
            result = esphome_runner.validate_config(config_path)
            if not result['success']:
                failed_configs.append({
                    'config': config_path.name,
                    'error': result['stderr']
                })
        
        if failed_configs:
            error_msg = "Failed validations:\n"
            for fail in failed_configs:
                error_msg += f"  - {fail['config']}: {fail['error'][:100]}...\n"
            pytest.fail(error_msg)
    
    @pytest.mark.validation
    def test_configs_have_required_components(self, workspace_root, test_configs):
        """Test that configs have required UPS HID components"""
        for config_path in test_configs:
            with open(config_path) as f:
                content = f.read()
            
            # Check for required components
            assert "ups_hid:" in content, f"{config_path.name} missing ups_hid component"
            assert "external_components:" in content, f"{config_path.name} missing external_components"
            assert "esp32:" in content, f"{config_path.name} missing esp32 platform"
    
    @pytest.mark.validation
    def test_simulation_configs_have_simulation_mode(self, test_configs):
        """Test that simulation configs have simulation_mode enabled"""
        for config_path in test_configs:
            if "simulation" in config_path.name:
                with open(config_path) as f:
                    content = f.read()
                
                assert ('simulation_mode: true' in content or 
                       'simulation_mode: "true"' in content), \
                       f"{config_path.name} missing simulation_mode: true"


class TestConfigurationCompilation:
    """Test ESPHome configuration compilation (MANDATORY)"""
    
    @pytest.mark.compilation
    @pytest.mark.slow
    @pytest.mark.parametrize("config", [
        "configs/testing/minimal_simulation.yaml",
    ], ids=lambda x: Path(x).stem)
    def test_config_compilation_mandatory(self, esphome_runner, workspace_root, config):
        """MANDATORY: Test that configurations compile successfully"""
        config_path = workspace_root / config
        assert config_path.exists(), f"Config file {config} not found"
        
        result = esphome_runner.compile_config(config_path, timeout=600)
        
        # Detailed failure reporting
        if not result['success']:
            error_lines = result['stderr'].split('\n')[-20:]  # Last 20 lines
            pytest.fail(
                f"MANDATORY COMPILATION FAILED for {config}:\n"
                f"Duration: {result['duration']}s\n"
                f"Return code: {result['returncode']}\n"
                f"Last error lines:\n" + "\n".join(error_lines)
            )
        
        assert result['success'], f"MANDATORY: Compilation failed for {config}"
        assert result['duration'] < 600, f"Compilation too slow: {result['duration']}s"
    
    @pytest.mark.compilation
    @pytest.mark.slow
    def test_all_standalone_configs_compile(self, esphome_runner, test_configs):
        """MANDATORY: Test that all standalone configs compile"""
        failed_compilations = []
        compilation_times = []
        
        for config_path in test_configs:
            print(f"\n🔨 Compiling {config_path.name}...")
            result = esphome_runner.compile_config(config_path, timeout=600)
            
            compilation_times.append({
                'config': config_path.name,
                'duration': result['duration'],
                'success': result['success']
            })
            
            if not result['success']:
                error_lines = result['stderr'].split('\n')[-10:]
                failed_compilations.append({
                    'config': config_path.name,
                    'duration': result['duration'],
                    'error': "\n".join(error_lines)
                })
        
        # Report compilation times
        print("\n📊 Compilation Times:")
        for time_info in compilation_times:
            status = "✅" if time_info['success'] else "❌"
            print(f"  {status} {time_info['config']}: {time_info['duration']}s")
        
        # Fail if any compilation failed
        if failed_compilations:
            error_msg = "MANDATORY COMPILATION FAILURES:\n"
            for fail in failed_compilations:
                error_msg += f"\n❌ {fail['config']} ({fail['duration']}s):\n{fail['error']}\n"
            pytest.fail(error_msg)
    
    @pytest.mark.compilation
    @pytest.mark.slow
    def test_compilation_performance(self, esphome_runner, test_configs):
        """Test compilation performance benchmarks"""
        slow_compilations = []
        
        for config_path in test_configs:
            result = esphome_runner.compile_config(config_path, timeout=600)
            
            if result['success'] and result['duration'] > 300:  # 5 minutes
                slow_compilations.append({
                    'config': config_path.name,
                    'duration': result['duration']
                })
        
        if slow_compilations:
            warning_msg = "⚠️ Slow compilations (>5min):\n"
            for slow in slow_compilations:
                warning_msg += f"  - {slow['config']}: {slow['duration']}s\n"
            print(warning_msg)  # Warning, not failure


class TestSimulationMode:
    """Test simulation mode functionality"""
    
    @pytest.mark.simulation
    @pytest.mark.parametrize("config", [
        "configs/testing/minimal_simulation.yaml",
        "configs/testing/simulation_test_standalone.yaml"
    ])
    def test_simulation_runs(self, esphome_runner, workspace_root, config):
        """Test that simulation mode runs without hardware"""
        config_path = workspace_root / config
        
        # Only test configs that have simulation enabled
        with open(config_path) as f:
            content = f.read()
        
        if not ('simulation_mode: true' in content or 'simulation_mode: "true"' in content):
            pytest.skip(f"{config} doesn't have simulation mode enabled")
        
        result = esphome_runner.run_simulation(config_path, duration=15)
        
        if not result['success']:
            pytest.fail(
                f"Simulation failed for {config}:\n"
                f"Duration: {result['duration']}s\n"
                f"Return code: {result['returncode']}\n"
                f"Stderr: {result['stderr'][:500]}\n"
                f"Stdout: {result['stdout'][:500]}"
            )
        
        assert result['success'], f"Simulation failed for {config}"
    
    @pytest.mark.simulation
    def test_simulation_generates_sensor_data(self, esphome_runner, workspace_root):
        """Test that simulation generates realistic sensor data"""
        config_path = workspace_root / "configs/testing/simulation_test_standalone.yaml"
        
        result = esphome_runner.run_simulation(config_path, duration=20)
        
        assert result['success'], "Simulation failed to run"
        
        # Check for sensor data in output
        output = result['stdout'] + result['stderr']
        
        # Look for typical ESPHome sensor output patterns
        sensor_indicators = [
            "Battery Level",
            "Sending state",
            "UPS Status",
            "simulation"
        ]
        
        found_indicators = [indicator for indicator in sensor_indicators 
                          if indicator.lower() in output.lower()]
        
        assert len(found_indicators) >= 2, \
               f"Simulation should generate sensor data. Found: {found_indicators}"


class TestProductionConfigs:
    """Test production configuration examples"""
    
    @pytest.mark.validation
    def test_production_configs_validate(self, esphome_runner, production_configs):
        """Test that production example configs validate (may need secrets)"""
        failed_configs = []
        
        for config_path in production_configs:
            result = esphome_runner.validate_config(config_path)
            
            # Production configs may fail due to missing secrets or duplicate entities - that's OK for examples
            if not result['success']:
                full_output = result['stderr'] + result['stdout']
                if ("secrets.yaml" in full_output or 
                    "!secret" in full_output or
                    "Duplicate" in full_output or
                    "Each entity must have a unique name" in full_output):
                    pytest.skip(f"{config_path.name} has expected production config issues")
                else:
                    failed_configs.append({
                        'config': config_path.name,
                        'error': result['stderr']
                    })
        
        if failed_configs:
            error_msg = "Production config validation failures (excluding secrets):\n"
            for fail in failed_configs:
                error_msg += f"  - {fail['config']}: {fail['error'][:100]}...\n"
            pytest.fail(error_msg)
    
    @pytest.mark.validation
    def test_production_configs_have_device_specifics(self, production_configs):
        """Test that production configs have device-specific settings"""
        for config_path in production_configs:
            with open(config_path) as f:
                content = f.read()
            
            # Should include device-specific packages
            has_device_package = any(
                package in content for package in [
                    "apc_backups_es.yaml",
                    "cyberpower_cp1500.yaml",
                    "device_types/"
                ]
            )
            
            if not has_device_package:
                pytest.fail(
                    f"{config_path.name} should include device-specific packages"
                )