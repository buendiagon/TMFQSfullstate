Error metric used for the thesis column: max_i |a_zfp(i) - a_dense(i)|
ZFP configuration: FixedPrecision, precision=40, chunkStates=32768, gateCacheSlots=8, nthreads=4
Reduced qubit sweep: 18 19 20

Grover objetivo unico
| Qubits | Error | Peor caso | L2 rel. | RMSE amplitud |
| --- | ---: | ---: | ---: | ---: |
| 18 | 0.000000e+00 | 0.000000e+00 | 0.000000e+00 | 0.000000e+00 |
| 19 | 4.446561e-14 | 4.446778e-14 | 2.266045e-11 | 3.129562e-14 |
| 20 | 3.218066e-14 | 3.218244e-14 | 2.534879e-11 | 2.475468e-14 |

Grover multiobjetivo
| Qubits | Error | L2 rel. | RMSE amplitud | Delta prob. total |
| --- | ---: | ---: | ---: | ---: |
| 18 | 0.000000e+00 | 0.000000e+00 | 0.000000e+00 | 0.000000e+00 |
| 19 | 4.446995e-14 | 2.282355e-11 | 3.152088e-14 | 3.996803e-15 |
| 20 | 3.218787e-14 | 2.541939e-11 | 2.482363e-14 | 3.330669e-15 |

QFT superposicion periodica
| Qubits | Error | L2 rel. | RMSE amplitud | Delta prob. total |
| --- | ---: | ---: | ---: | ---: |
| 18 | 3.354867e-12 | 8.176434e-12 | 1.596960e-14 | 4.196643e-12 |
| 19 | 3.785789e-11 | 1.333940e-10 | 1.842262e-13 | 4.851675e-11 |
| 20 | 3.785789e-11 | 1.333940e-10 | 1.302676e-13 | 4.851719e-11 |

QFT alta entropia
| Qubits | Error | L2 rel. | RMSE amplitud | Delta prob. total |
| --- | ---: | ---: | ---: | ---: |
| 18 | 1.780425e-12 | 7.076911e-12 | 1.382209e-14 | 1.110223e-15 |
| 19 | 2.449474e-11 | 2.576707e-10 | 3.558607e-13 | 1.971756e-13 |
| 20 | 3.289745e-11 | 2.616768e-10 | 2.555437e-13 | 3.540501e-13 |

