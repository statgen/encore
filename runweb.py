from encore import create_app

import os
config = os.path.join(os.path.dirname(os.path.abspath(__file__)), "flask_config.py")
print(config)
app = create_app(config)
