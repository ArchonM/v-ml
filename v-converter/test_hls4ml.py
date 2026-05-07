import torch
import torch.nn as nn
import hls4ml
import numpy as np

# ---------------------------------------------------------
# 1. Build a Tiny PyTorch Model
# ---------------------------------------------------------
print("Building and initializing PyTorch model...")

# Define a 3-layer network (16 inputs -> 32 -> 16 -> 1 output)
class TinyModel(nn.Module):
    def __init__(self):
        super(TinyModel, self).__init__()
        self.fc1 = nn.Linear(16, 32)
        self.relu1 = nn.ReLU()
        self.fc2 = nn.Linear(32, 16)
        self.relu2 = nn.ReLU()
        self.fc3 = nn.Linear(16, 1)
        self.sigmoid = nn.Sigmoid()

    def forward(self, x):
        x = self.relu1(self.fc1(x))
        x = self.relu2(self.fc2(x))
        x = self.sigmoid(self.fc3(x))
        return x

model = TinyModel()

# Generate some dummy data and do a quick forward pass to initialize
# (We are skipping training here just to show the bare-bones conversion)
dummy_input = torch.rand(10, 16)
_ = model(dummy_input)

# CRITICAL STEP: Set the model to evaluation mode before conversion
model.eval()

# ---------------------------------------------------------
# 2. Configure hls4ml for Arty A7 (xc7a100tcsg324-1)
# ---------------------------------------------------------
print("Configuring hls4ml...")

# CRITICAL STEP: PyTorch requires explicit input shapes.
# The shape is formatted as [Batch_Size, Number_of_Features]
input_shape = (None, 16)

config = hls4ml.utils.config_from_pytorch_model(model, input_shape=input_shape, granularity='Model')

# Set data types (ap_fixed<16,6> means 16 bits total, 6 integer bits)
config['Model']['Precision'] = 'ap_fixed<16,6>'
config['Model']['ReuseFactor'] = 16

output_dir = 'arty_a7_pytorch_project'

# Initialize the converter with the PyTorch model and Vitis backend
hls_model = hls4ml.converters.convert_from_pytorch_model(
    model,
    input_shape=input_shape, 
    hls_config=config,
    output_dir=output_dir,
    backend='Vitis',
    part='xc7a100tcsg324-1',
    clock_period=10.0
)

# ---------------------------------------------------------
# 3. Generate C++ Files 
# ---------------------------------------------------------
print("Compiling HLS model...")
# hls_model.compile()
hls_model.write()

print(f"\nSuccess! PyTorch model translated to C++ in '{output_dir}'.")
print("To synthesize into Verilog, run the following in your terminal:")
print(f"  cd {output_dir}")
print("  vitis-run --mode hls --tcl build_prj.tcl")

# Generate C++ project, but run synthesis manually via terminal