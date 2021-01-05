import plotly.graph_objs as go
import plotly.offline as py

from . import meas_xrd


def plot_xrd(meas: meas_xrd.MeasXRD):
    trace = go.Scatter(x=meas.data["Angle"], y=meas.data["Intensity"])

    layout = go.Layout(title="XRD data", xaxis=dict(title="Angle"), yaxis=dict(title="Intensity", type="log"))

    data = [trace]
    fig = go.Figure(data=data, layout=layout)
    return py.plot(fig, output_type="div", include_plotlyjs=False)
